/*	procmgr - This routine will be in charge of starting and stopping 
	          certain programs. The programs being controlled will be
	          located in a file ("pmgrtbl.dat"). Below is the format
	          of this file.

		1-64	Full path name of program
		65	Flag to send message when program is up 1 = yes
		66-69	Message queue number to use on shutdown
		70-133	Current working directory
		134	Flag whether process is included in "ALL" 1 = yes
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
		cc.10 procmgr.c -oprocmgr
		copy procmgr to /IMOSDATA/5
		copy procmgr to /SOURCE/TAPE.DIR/05.08.00/IMOSDATA/5
	For the 825:
		cc procmgr.c -oprocmgr
		copy procmgr to /SOURCE/TAPE.DIR/825.050800/IMOSDATA/5

*/
/* JLS - 01/30/96 Attempt at fixing the COBOL programs.  Changed the execl
         statement for them to a system command so that we can string all
         of the nohup and -sb parameters to be added to the command.
	MJB - 06/02/03 Changed macro to create grepman in /tmp
*/

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXPRG 15
#define FSTRING1 "ps -ef | grep '"
#define FSTRING2 "' | grep -v 'grep' | grep -v 'UP' > grepman"

struct msgbuffs {	/**** Buffer used for programs message queue */
   long mtype;
   long command;
   char fname[64];
   char user[16];
   char qname[8];
   long status;
   int rtnque;
} msgque;

struct msgargs {	/* Buffer used for programs with arguments */
   long mtype;
   char marg1[9];
   char marg2[9];
   char marg3[9];
} msga;

struct msgbuff {	/*** Buffer used for process managers */
   long mtype;		/*** message queue */
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
} msg;

char runcblpathname[100];
char str_systemname[140];

char proctbl[32][9];
int pidtbl[32];
int stattbl[32];
int catcher();

main()
{
struct sigaction sa;
char bldstg[100], buff[513];
char *getenv(), *qdname;
char fnames[256];

long flg;
int i, j, curqid, msqid, prgcnt=0, curptr, endpos=0;
int retval, retryit, cnt,newpid;

struct msqid_ds msqbuf;
FILE *dsktable;

/* Get name of the QDATA directory */

sa.sa_handler = catcher;
sigemptyset(&sa.sa_mask);
sigaddset(&sa.sa_mask, SIGCHLD);
sa.sa_flags = 0;
sigaction(SIGCHLD, &sa, NULL);

for (i=0;i<32;i++) {
   proctbl[i][0] = 0;
   stattbl[i] = 0;
   pidtbl[i] = -1;
}
if ((qdname = getenv("QDATA")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit (0);
}

/* Create the queue that will be used to send and receive messages in this
   program. */
top:
if ((msqid = msgget (1001,IPC_CREAT | IPC_EXCL | 0666)) < 0) {
   if (errno == EEXIST) {
	msqid = msgget(1001,0666);
	if ((i = msgctl(msqid,IPC_RMID,&msqbuf)) < 0) {
	   perror ("MSGGET delete privileges - procmgr");
	   exit (0);
	}
	goto top;
   }
   perror ("MSGGET - procmgr");
   exit (0);
}

/* Sit and wait for a message. The message should tell us what needs to be
   done. Receive only message type 1, the other types will be picked up
   elsewhere. */

signal(SIGCLD,(void(*)(int))catcher);
waitmore:
if (msgrcv(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),1,0) < 0) {
   if (errno == EINTR) goto waitmore;
   perror ("MSGRCV - procmgr");
   goto waitmore;
}

if (msg.status == 99) goto shutdown;

/* If there arguments to be received, receive them now */

if ((strcmp(msg.updwn,"1")) != 0 && msg.args > 0) {
getargs:
   if (msgrcv(msqid,&msga,(sizeof(msga)-sizeof(msga.mtype)),2,0) <0) {
        if (errno == EINTR) goto getargs;
	perror ("MSGRCV2 - procmgr");
	msg.status = 98;
	goto skipit;
   }
}

/* START the current program named in the message */
sprintf(str_systemname, "nohup /usr/lbin/runcbl -sb ");
strcat(str_systemname, msg.pathname);

/* sprintf(nohuppathname,"/usr/bin/nohup");  */
sprintf(runcblpathname,"/usr/lbin/runcbl");
if ((strcmp(msg.updwn,"1")) != 0) {
   for (i=0;pidtbl[i] != -1;i++);
   if (msg.args == 0) {
        if ((newpid = fork()) == 0) {
	   sprintf(fnames,"%s/%s.out",qdname,msg.progmnem);
	   fclose(stdout);
           (FILE *)fopen(fnames,"w+");
           fclose(stderr);
           (FILE *)fopen(fnames,"w+");
           strcat(str_systemname, "&");

	   execl(msg.pathname,msg.pathname,0);
           /*  execl(nohuppathname,runcblpathname,runcblpathname,msg.pathname,0);    */
           system(str_systemname);
           /*  perror("EXECL - procmgr");  */

           /*  exit(1);  */
           exit(0);
        }
        else {
           strcpy(proctbl[i],msg.progmnem);
           pidtbl[i] = newpid;
           stattbl[i] = 1;
        }
   }
   else {
        if ((newpid = fork()) == 0) {
	   sprintf(fnames,"%s/%s.out",qdname,msg.progmnem);
	   fclose(stdout);
           (FILE *)fopen(fnames,"w+");
           fclose(stderr);
           (FILE *)fopen(fnames,"w+");
	   if (msg.args == 1) {
             execl(msg.pathname,msg.pathname,msga.marg1,0);
             /*  execl(nohuppathname,runcblpathname,runcblpathname,msg.pathname,msga.marg1,0);  */
             strcat(str_systemname, " ");
             strcat(str_systemname, msga.marg1);
             strcat(str_systemname, "&");

             system(str_systemname);

             /*  perror("EXECL - procmgr");  */
             /*  exit(1);  */
             exit(0);
           }
           else if (msg.args == 2) {
             execl(msg.pathname,msg.pathname,msga.marg1,msga.marg2,0);
             /*  execl(nohuppathname, runcblpathname,runcblpathname,msg.pathname,msga.marg1,msga.marg2,0);  */
             strcat(str_systemname, " ");
             strcat(str_systemname, msga.marg1);
             strcat(str_systemname, " ");
             strcat(str_systemname, msga.marg2);
             strcat(str_systemname, "&");

             system(str_systemname);

             /*  perror("EXECL - procmgr");  */
             /*  exit(1);  */
             exit(0);
           }
           else {
            execl(msg.pathname,msg.pathname,msga.marg1,msga.marg2,msga.marg3,0);
            /*  execl(nohuppathname,runcblpathname,runcblpathname,msg.pathname,msga.marg1,msga.marg2,msga.marg3,0);  */
            strcat(str_systemname, " ");
            strcat(str_systemname, msga.marg1);
            strcat(str_systemname, " ");
            strcat(str_systemname, msga.marg2);
            strcat(str_systemname, " ");
            strcat(str_systemname, msga.marg3);
            strcat(str_systemname, "&");

            system(str_systemname);

            /*  perror("EXECL - procmgr");  */
            /*  exit(1);  */
            exit(0);
           }
        }
        else {
           strcpy(proctbl[i],msg.progmnem);
           pidtbl[i] = newpid;
           stattbl[i] = 1;
        }
   }
sleeper:
   if (strncmp(proctbl[i],"qmgr",4) == 0) {
qmgr1:
      if (msgget(1000,0666) < 0) {
         sleep(3);
         if (stattbl[i] == 0) {
            msg.status = 98;
            goto sndup;
         }
         goto qmgr1;
      }
      else {
         msg.status = 0;
         goto sndup;
      }
   }
   else
   if (sleep (15) != 0) {
      if (stattbl[i] == 0) {
	 msg.status = 98;
         goto sndup;
      }
    }

   retryit=0;
   msg.status = 0;

sndup:
   /* Send message to proccntl reporting the status of the program. 
      Use message type 3, "proccntl" will pick this message up. */

   msg.mtype = 3; 
   if (msgsnd(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
	perror ("MSGSND - procmgr");
   }
   goto waitmore;
}

/* STOP the current program if there is a message queue for it. */

if ((strcmp(msg.updwn,"1")) == 0 && (strcmp(msg.quenum,"0000")) != 0) {
   for (i=0;i<32;i++) {
      if (strcmp(msg.progmnem,proctbl[i]) == 0) {
          break;
      }
   }
   stattbl[i] = 0;
   msg.status = 0;
   flg = atoi(msg.quenum);
   if ((curqid = msgget (flg,0666)) < 0) {
      kill(pidtbl[i],16);
      goto sndpctl;
   }

   /* Send message to current program to shutdown. Use message type 3, the
      program running should pick up this message. If this program shuts
      down via the Queue Manager then we send a command of 98. The Queue
      Manager will then send the appropriate program a return status of 
      an "A". The program should then proceed to shutdown. */

   msgque.command = 98;
   strcpy(msgque.qname,msg.quename);
   msgque.status = atoi(msg.rdwrt);
   if ((strcmp(msg.progmnem,"qmgr")) == 0) msgque.command = 99;

   msgque.mtype = 3;
   if (msgsnd(curqid,&msgque,
             (sizeof(msgque)-sizeof(msgque.mtype)),0) < 0) {
	perror ("MSGSND - procmgr");
	goto skipit;
   }


   /* Send message to proccntl reporting the status of the program. 
      Use message type 3, "proccntl" will pick this message up. */
sndpctl:
   msg.mtype = 3; 
   if (msgsnd(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
	perror ("MSGSND - procmgr");
   }
   goto waitmore;
}

/* STOP the program with no message queue associated with it */

if ((strcmp(msg.updwn,"1")) == 0 && (strcmp(msg.quenum,"0000")) == 0) {
   for (i=0;i<32;i++) {
      if (strcmp(msg.progmnem,proctbl[i]) == 0) {
         stattbl[i] = 0;
         kill(pidtbl[i],16);

	/* Send message to proccntl reporting the program is down.
	   Use message type 3, "proccntl" will pick this message up. */

	msg.mtype = 3; 
	if (msgsnd(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
	   perror ("MSGSND - procmgr");
	}
	goto waitmore;
      }
   }
}
puts("INVALID REQUEST FOR PROCMGR");
msg.status = 98;

/* Send message to proccntl reporting there was an error. 
   Use message type 3, "proccntl" will pick this message up. */

skipit:
msg.mtype = 3; 
if (msgsnd(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   perror ("MSGSND - procmgr");
}
goto waitmore;

shutdown:
msgctl(msqid,IPC_RMID,&msqbuf);
exit(0);
}

int catcher()
{
int inpid;
int k,l;

/* inpid = wait((int *) 0); */
while ((inpid = waitpid(-1, NULL, WNOHANG)) > 0) {
   for (k=0;k<32;k++) {
      if (pidtbl[k] == inpid) {
         if (strncmp("qmgr",proctbl[k],4) == 0) {
            proctbl[k][0] = 0;
            pidtbl[k] = -1;
            stattbl[k] = 0;
            for (l=0;l<32;l++) {
               if (pidtbl[l] != -1) {
                  kill(pidtbl[l],16);
                  pidtbl[l] = -1;
                  proctbl[l][0] = 0;
                  stattbl[l] = 0;
               }
            }
//            goto donesig;
            break;
         }
         else if (stattbl[k] == 0) {
            pidtbl[k] = -1;
            proctbl[k][0] = 0;
 //           goto donesig;
            break;
         }
         else {
            pidtbl[k] = -1;
            stattbl[k] = 0;
            proctbl[k][0] = 0;
//            goto donesig;
            break;
         }
      }
   }
}
donesig:
/*signal(SIGCLD,(void(*)(int))catcher);*/
return(0);
}
