#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/filio.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#include <signal.h>
#else
#include <siginfo.h>
#endif


#include "common.h"
#include "maintq.h"
#include "maint.h"
#include "sub.h"
#include "westcp.h"
#include "maintc.h"
#include "err.h"

#ifndef NETWORKLOG
#define NETWORKLOG (-1)
#endif

int shmid;
MSQDATA *prgdata;
struct sockaddr_in myaddr;
int s;
int ls;
int db;
char logfile[FILENAMELENGTH];
int debugsocket;
char *progname;
sigset_t set;
struct sigaction chldact;
struct sigaction usr1act;
mode_t oldmask;
int imdone = 0;
int seminit[2] = { 0, 0 };
int semid;
char *wescomini = WESCOMINI;
struct sembuf locksem[] = {
    { 0, 0, SEM_UNDO }, /* wait for semaphore #0 == 0  */
    { 0, 1, SEM_UNDO }, /* increment semaphore #0 by 1 */
};

void AcceptConnections(void);
int qmailmsg(char *, ...);
void myshutdown(int);
void myfuneral(int);
void logshutdown(void);
int setsignal(int);
void logsignal(int, struct siginfo *);
void sigio_handler(int);

char * cblconfig = NULL;

main(int argc, char *argv[])
{
    int option;
    short flag = 0;
    short one = 1;
    char dbfilename[FILENAMELENGTH];
    extern char *optarg;
	 sigset_t signal_mask;

    oldmask = umask(~(S_IRWXO|S_IRWXG|S_IRWXO));

    progname = argv[0];

    GetPrivateProfileString(MQ_INI_SECTION, "debugfile", dbfilename,
        sizeof(dbfilename), DBG_FILENAME, wescomini);
    GetPrivateProfileString(MQ_INI_SECTION, "translog", logfile,
        sizeof(logfile), LOG_FILENAME, wescomini);

    if ((db = creat(dbfilename, 0666)) < 0)
        err_sys("error creating %s", dbfilename);

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGUSR1);

    while ((option = getopt(argc, argv, "c:fi:st")) != EOF) {
        switch (option) {
            case 'c':
                cblconfig = optarg;
                break;
            case 'f':
                flag |= Q_DEBUGLOG;
                break;
            case 'i':
                wescomini = optarg;
                break;
            case 's':
                flag |= Q_DEBUGSOCKET;
                break;
            case 't':
                flag |= Q_LOGTRANS;
                break;
            default:
                printf("usage: maintserverQ -fst\n\n");
                exit(1);
        }
    }

    prgdata = (MSQDATA *)GetSharedMemory(&shmid, MQ_SHM_KEY, sizeof(MSQDATA),
        IPC_CREAT|READWRITE);
    if (prgdata == NULL)
        err_sys("can not initialize shared memory");

    prgdata->status = MSQ_INITIALIZING;
    prgdata->debuglog = (flag & Q_DEBUGLOG);
    if (prgdata->debuglog)
        wescolog(db, "%d: debug logging is on\n", getpid());

    prgdata->logtransactions = (flag & Q_LOGTRANS);
    if (prgdata->debuglog)
        wescolog(db, "%d: transaction logging is %s\n", getpid(),
            (prgdata->logtransactions) ? "on" : "off");

    prgdata->debugsocket = (flag & Q_DEBUGSOCKET);
    if (prgdata->debuglog)
        wescolog(db, "%d: socket debugging is %s\n", getpid(),
            (prgdata->debugsocket) ? "on" : "off");

    prgdata->mailedmsgs = 0;
    prgdata->numconnections = 0;
    prgdata->timeout = (short)GetPrivateProfileInt(MQ_INI_SECTION, "timeout",
        30, wescomini);
    if (prgdata->debuglog)
        wescolog(db, "%d: timeout=%d\n", getpid(), prgdata->timeout);

    prgdata->maxconnections = (short)GetPrivateProfileInt(MQ_INI_SECTION,
        "maxconnections", 32, wescomini);
    if (prgdata->debuglog)
        wescolog(db, "%d: maxconnections=%d\n", getpid(),
            prgdata->maxconnections);

    GetPrivateProfileString("IQ", "mail-to", prgdata->mailto,
        sizeof(prgdata->mailto), "root", wescomini);
    if (prgdata->debuglog)
        wescolog(db, "%d: mail-to=%s\n", getpid(), prgdata->mailto);

    GetPrivateProfileString("IQ", "service", prgdata->service,
        sizeof(prgdata->service), "WDOCUPDQ", wescomini);
    if (prgdata->debuglog)
        wescolog(db, "%d: service=%s\n", getpid(), prgdata->service);

    GetPrivateProfileString("IQ", "protocol", prgdata->protocol,
        sizeof(prgdata->protocol), "tcp", wescomini);
    if (prgdata->debuglog)
        wescolog(db, "%d: protocol=%s\n", getpid(), prgdata->protocol);

    if (prgdata->debuglog)
        wescolog(db, "%d: shared memory initialization complete\n", getpid());

    if (initaddress(&myaddr, prgdata->service, prgdata->protocol) < 0) {
        wescolog(NETWORKLOG,
            "%s: %s not found in /etc/services");
        err_sys("error on service lookup");
    }

    if (prgdata->debuglog)
        wescolog(db, "%d: socket address initialization complete\n", getpid());

    if ((ls = getlistensocket(&myaddr, MAXRECEIVES)) < 0) {
        fprintf(stderr, "getlistensocket() = %d\n", ls);
        err_sys("error creating listen socket");
    }

    if (prgdata->debuglog)
        wescolog(db, "%d: listen socket created\n", getpid());

    if (prgdata->debugsocket)
        setsocketdebug(ls, 1);

    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));

    daemoninit(argv[0]);
    prgdata->pid = getpid();

    if (prgdata->debuglog)
        wescolog(db, "%d: daemon initialization complete\n", prgdata->pid);

    setsignal(SIGABRT);
    setsignal(SIGALRM);
    setsignal(SIGBUS);
#if defined(SIGEMT)
    setsignal(SIGEMT);
#endif

    setsignal(SIGFPE);
    setsignal(SIGILL);
    setsignal(SIGPROF);
    setsignal(SIGQUIT);
    setsignal(SIGPIPE);
    setsignal(SIGSEGV);
    setsignal(SIGSYS);
    setsignal(SIGTERM);
    setsignal(SIGTRAP);
    setsignal(SIGUSR2);
    setsignal(SIGVTALRM);
    setsignal(SIGXCPU);
    setsignal(SIGXFSZ);

    atexit(logshutdown);

    AcceptConnections();
}

void AcceptConnections(void)
{
	int addrlen, retcode;
	char version[VERSIONLENGTH+1], *hostname;
	int status;
	struct sockaddr_in peeraddr;
	struct hostent *hp;
	struct servent *sp;
	int count;
	int statloc;
	int serrno;
	char *msg;
	key_t key;
	int flags;
	short one = 1;
	u_short so_errno;
	size_t ss;
	sigset_t process_mask;
	sigset_t block_mask;

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGCHLD);

	key = MQ_SEMAPHORE_KEY;
	flags = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	if ((semid = semget(key, 1, flags)) < 0) {
		flags |= IPC_CREAT;
		if ((semid = semget(key, 1, flags)) < 0) {
			perror(MQ_INI_SECTION);
			exit(1);
	   }

	   if (semctl(semid, 0, SETALL, seminit) < 0) {
		   perror(MQ_INI_SECTION);
		   exit(1);
	   }
	}

	/* set signal handlers */
	chldact.sa_handler = myfuneral;
	sigemptyset(&chldact.sa_mask);
	sigaddset(&chldact.sa_mask, SIGALRM);
	sigaddset(&chldact.sa_mask, SIGUSR1);
	chldact.sa_flags = 0;
	chldact.sa_flags = SA_NOCLDSTOP|SA_RESTART;
	sigaction(SIGCHLD, &chldact, NULL);

	usr1act.sa_handler = myshutdown;
	sigemptyset(&usr1act.sa_mask);
	sigaddset(&usr1act.sa_mask, SIGALRM);
#ifdef SVR4
	sigaddset(&usr1act.sa_mask, SIGCLD);
#else
	sigaddset(&usr1act.sa_mask, SIGCHLD);
#endif
	usr1act.sa_flags = 0;
	sigaction(SIGUSR1, &usr1act, NULL);

	prgdata->status = MSQ_RUNNING;

	sigemptyset(&process_mask);
	sigaddset(&process_mask, SIGHUP);
	sigaddset(&process_mask, SIGINT);
	sigaddset(&process_mask, SIGQUIT);
	sigaddset(&process_mask, SIGIO);
	sigaddset(&process_mask, SIGPIPE);

	for ( ; ; ) {
		sigprocmask(SIG_SETMASK, &process_mask, (sigset_t *)NULL);

		if (prgdata->debuglog)
			wescolog(db, "%d: ready for next connection\n", prgdata->pid);

		addrlen = sizeof(peeraddr);
		s = accept(ls, (struct sockaddr *)&peeraddr, &addrlen);
		sigprocmask(SIG_BLOCK, &block_mask, &process_mask);
		if (s < 0) {
			if (prgdata->status == MSQ_SHUTTINGDOWN) {
				prgdata->status = MSQ_TERMINATED;
				exit(0);
			}
			else {
				if ((errno == EINTR) || (errno == EPROTO) ||
					(errno == ECONNABORTED))
				   continue;

				close(ls);
				serrno = errno;
				wescolog(NETWORKLOG, "%s: unable to accept connection, %d", 
					progname, serrno);

				ss = sizeof(so_errno);
				getsockopt(ls, SOL_SOCKET, SO_ERROR, (char *)&so_errno, (int *)&ss);

				if (prgdata->debuglog)
				   wescolog(db, "%d: error on accept(); errno = %d\n",
					  prgdata->pid, serrno);

				while ((ls = getlistensocket(&myaddr, MAXRECEIVES)) < 0) {
				   if (prgdata->status == MSQ_SHUTTINGDOWN) {
					  if (prgdata->debuglog)
						 wescolog(db, "%d: shutting down\n", getpid());
					  while (wait(&statloc) > 0);
					  prgdata->status = MSQ_TERMINATED;
					  exit(0);
				   }

				   serrno = errno;

				   switch (ls) {
					  case -1:
						 msg = "error creating socket";
						 break;
					  case -2:
						 msg = "error binding socket";

						 /* we can try to reset the address for the server */
						 initaddress(&myaddr, prgdata->service,
							prgdata->protocol);
						 break;
					  case -3:
						 msg = "error setting listen buffer";
						 break;
					}

					wescolog(NETWORKLOG, "%s: %s; errno = %d", progname,
					   msg, serrno);

				   if (prgdata->debuglog)
					  wescolog(db, "%d: %s; errno = %d\n", prgdata->pid, msg,
						 serrno);

				   sleep(5);  /* give it a rest */
				}

				if (prgdata->debugsocket)
					setsocketdebug(ls, 1);

				setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));

				if (prgdata->debuglog)
				   wescolog(db, "%d: new listen socket created\n",
					  prgdata->pid);

				sleep(1);
			}
			continue;
		}

		  if (semop(semid, locksem, 2) < 0) {
				/* can't lock the semaphore, go away */
				close(s);
				continue;
		  }

		  if (prgdata->numconnections < prgdata->maxconnections)
				prgdata->numconnections++;
		else {
			if (prgdata->debuglog)
			   wescolog(db, "%d: too many current connections\n", getpid());

			semctl(semid, 0, SETVAL, 0);
			senddata(s, "\ff", 1, 0);
			close(s);
			continue;
		}

		semctl(semid, 0, SETVAL, 0);
		if (prgdata->debuglog)
			wescolog(db, "%d: number of connections increased to %d\n",
				getpid(), prgdata->numconnections);

		retcode = fork();
		switch (retcode) {
			case -1:
				serrno = errno;
				wescolog(NETWORKLOG, "%s: unable to fork daemon; errno = %d",
					 progname, serrno);

				if (prgdata->debuglog)
				   wescolog(db, "%d: error on fork(); errno = %d\n",
					  prgdata->pid, serrno);

				sleep(1);
				break;
			case 0:
				close(ls);
				setsid();

				if (prgdata->debuglog)
				   wescolog(db, "%d: authorizing connection\n", getpid());

				if (AuthorizeConnection(s, &hp, &peeraddr)) {
#if 0
					if (semop(semid, locksem, 2) < 0) {
						perror(MQ_INI_SECTION);
						exit(SEM_EXIT);
					}

					if (prgdata->numconnections < prgdata->maxconnections)
						prgdata->numconnections++;
					else {
						if (prgdata->debuglog)
							wescolog(db, "%d: too many current connections\n",
								getpid());

						semctl(semid, 0, SETVAL, 0);
						senddata(s, "\ff", 1, 0);
						exit(SEM_EXIT);
					}
#endif

					hostname = hp->h_name;
					if (prgdata->debuglog) {
					   wescolog(db, "%d: connection authorized\n", getpid());
					   wescolog(db, "%d: getting version number\n", getpid());
					}

					memset(version, 0, sizeof(version));
					if (GetVersion(s, version, progname, hostname)) {
						if (prgdata->debuglog)
							wescolog(db, "%d: rx'd version %s\n",
							   getpid(), version);

						for (count = 0; count < NUMVERSIONSUPPORTED; count++) {
							if (!strcmp(msqversions[count].number, version)) {
								if (prgdata->debuglog)
									wescolog(db, "%d: calling server\n",
									   getpid());

								senddata(s, "\xff", 1, 0);
								msqversions[count].Server(hostname,
									msqversions[count].cobolProgram, 1024);
								close(s);
								if (prgdata->debuglog)
									wescolog(db, "%d: returned from server\n",
									   getpid());

								break;
							}
							else if (count == NUMVERSIONSUPPORTED - 1)
								senddata(s, "\0", 1, 0);
						}
					}
				}
				else {
					hostname = inet_ntoa(peeraddr.sin_addr);
					wescolog(NETWORKLOG,
						"%s: host at address %s unknown.  Disconnect!\n",
						hostname);

					if (prgdata->debuglog)
						wescolog(db, "%d: connection with %s not authorized\n",
							getpid(), hostname);

					close(s);
#if 0
						  exit(SEM_EXIT);
#else
						  exit(0);
#endif
				}

				if (prgdata->debuglog)
					wescolog(db, "%d: goodbye cruel world\n", getpid());

				exit(0);
				break;
			default:
				if (prgdata->debuglog)
				   wescolog(db, "%d: forked child %d\n", getpid(), retcode);

				if (prgdata->status == MSQ_SHUTTINGDOWN) {
					if (prgdata->debuglog)
					   wescolog(db, "%d: shutting down\n", getpid());
					while (wait(&statloc) > 0);
					prgdata->status = MSQ_TERMINATED;
					exit(0);
				}

				close(s);
		}
	}
}

OLDMNTMESSAGE clientmsg;

void MSQ060400(char *hostname, char *cobolfunc, int bufsize)
{
    short blksize;
    RPTDATA rpt;
    int count;
    Argument cobolargs[1];
    char *cobol_argv[3];
    int cobol_argc;
    extern int A_call_err;
    time_t starttime;
    int offs;

    if (prgdata->debuglog)
        wescolog(db, "cobol_argv[0]=%s; cobol_argv[1]=%s\n", progname,
            cobolfunc);

    if (prgdata->debuglog)
        wescolog(db, "%d: acu_initv completed\n", getpid());

    memset(&clientmsg, 32, sizeof(clientmsg));

    if (prgdata->debuglog)
        wescolog(db, "%d: %s has connected successfully.\n", getpid(), 
           hostname);

    if (recvdata(s, &blksize, sizeof(blksize), 0) < 0) {
        wescolog(NETWORKLOG, "%s: %s disconnected unexpectedly", progname,
            hostname);
        close(s);
        return;
    }

    if (prgdata->debuglog)
        wescolog(db, "%d: receiving %d records\n", getpid(), blksize);

    cobol_argv[0] = progname;
    cobol_argv[1] = cobolfunc;
    cobol_argc = 2;
    acu_initv(cobol_argc, cobol_argv);

    for (count = 0; count < blksize; count++) {
        offs = count%MAXRECORDS;
        if (recvdata(s, &clientmsg.lineitem[offs],sizeof(OLDLINEITEM),0) < 0) {
            wescolog(NETWORKLOG, "%s: %s disconnected unexpectedly", 
                progname, hostname);
            acu_shutdown(1);
            close(s);
            return;
        }

        cobolargs[0].a_address = (char *)&clientmsg;
        cobolargs[0].a_length = sizeof(clientmsg);

        if (prgdata->debuglog)
            wescolog(db, "%c%c%c%c\n", clientmsg.lineitem[offs].branch[0],
                clientmsg.lineitem[offs].branch[1],
                clientmsg.lineitem[offs].branch[2],
                clientmsg.lineitem[offs].branch[3]);

        if (prgdata->logtransactions) {
            memset(&rpt, 0, sizeof(rpt));
            strncpy(rpt.branch, clientmsg.lineitem[offs].branch, 
                sizeof(clientmsg.lineitem[offs].branch));
            strncpy(rpt.sku, clientmsg.lineitem[offs].sku,
                sizeof(clientmsg.lineitem[offs].sku));
            time(&starttime);
            rpt.starttime = ctime(&starttime);
            rpt.starttime[strlen(rpt.starttime) - 1] = 0;
            logit(&rpt);
        }

        if ((offs == 99) || (count == (blksize - 1))) {
            if (prgdata->debuglog)
                wescolog(db, "running %s\n", cobolfunc);

            if (cobol(cobolfunc, 1, cobolargs)) {
                wescolog(NETWORKLOG,
                    "%s: error running COBOL program %s, err = %d",
                    progname, cobolfunc, A_call_err);

                if (!(prgdata->mailedmsgs & MSG_COBOLERROR)) {
                    qmailmsg("%s: error running COBOL program %s, err = %d\n",
                        progname, cobolfunc, A_call_err);
                    prgdata->mailedmsgs |= MSG_COBOLERROR;
                }
             }

             if ((offs == 99) && (count != blksize)) {
                 memset(&clientmsg, 32, sizeof(clientmsg));
                 clientmsg.status[0] = clientmsg.status[1] = '0';
             }
        }
    }

    if (prgdata->debuglog)
        wescolog(db, "cobol returned %c%c\n", clientmsg.status[0],
            clientmsg.status[1]);

    senddata(s, &clientmsg, sizeof(clientmsg), 0);
    acu_shutdown(1);
    close(s);
}

void MSQ0605P1(char *hostname, char *cobolfunc, int bufsize)
{
    MSQ060400(hostname, cobolfunc, bufsize);
}

MNTMESSAGE clientmsg653;

void MSQ060503(char *hostname, char *cobolfunc, int bufsize)
{
   short blksize;
   RPTDATA rpt;
   int count;
   Argument cobolargs[1];
   char *cobol_argv[5];
   int cobol_argc;
   extern int A_call_err;
   time_t starttime;
   int offs;
   char buf[25];
   int done = 0;
   short i;
   char errormsg[25];
	int flags;
	struct sigaction sa;

   if (prgdata->debuglog)
      wescolog(db, "%d: cobol_argv[0]=%s; cobol_argv[1]=%s\n", getpid(),
         progname, cobolfunc);

   if (prgdata->debuglog)
      wescolog(db, "%d: acu_initv completed\n", getpid());

   memset(&clientmsg653, 32, sizeof(clientmsg653));

   if (prgdata->debuglog)
      wescolog(db, "%d: %s has connected successfully.\n", getpid(), hostname);

   cobol_argv[0] = progname;
   cobol_argv[1] = cobolfunc;

   if (cblconfig != NULL) {
      cobol_argv[2] = "-c";
      cobol_argv[3] = cblconfig;
      cobol_argc = 4;
   }
   else {
      cobol_argc = 2;
   }

   acu_initv(cobol_argc, cobol_argv);

   while (!done) {
      if (recvdata(s, &blksize, sizeof(blksize), 0) < 0) {
         wescolog(NETWORKLOG, "%s: %s disconnected unexpectedly", progname,
            hostname);
         acu_shutdown(1);
         close(s);
         return;
      }

      if (prgdata->debuglog)
         wescolog(db, "%d: receiving %d records\n", getpid(), blksize);

      if (blksize == 0) {
         done = 1;
         continue;
      }

      if (recvdata(s, &clientmsg653, sizeof(clientmsg653), 0) < 0) {
         wescolog(NETWORKLOG, "%s: %s disconnected unexpectedly", 
            progname, hostname);
         acu_shutdown(1);
         close(s);
         return;
      }

      if (prgdata->logtransactions) {
         for (i = 0; i < blksize; i++) {
            memset(&rpt, 0, sizeof(rpt));
            strncpy(rpt.branch, clientmsg653.lineitem[i].branch, 
               sizeof(clientmsg653.lineitem[i].branch));
            strncpy(rpt.sku, clientmsg653.lineitem[i].sku,
               sizeof(clientmsg653.lineitem[i].sku));
            time(&starttime);
            rpt.starttime = ctime(&starttime);
            rpt.starttime[strlen(rpt.starttime) - 1] = 0;
            logit(&rpt);
         }
      }

      cobolargs[0].a_address = (char *)&clientmsg653;
      cobolargs[0].a_length = sizeof(clientmsg653);

      if (prgdata->debuglog)
         wescolog(db, "%d: running COBOL program\n", getpid());

		sa.sa_handler = sigio_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGIO, &sa, (struct sigaction *)NULL);
		sigaction(SIGURG, &sa, (struct sigaction *)NULL);

		flags = 1;
		fcntl(s, F_SETOWN, getpid());
		ioctl(s, FIOASYNC, &flags);

      if (cobol(cobolfunc, 1, cobolargs)) {
         wescolog(NETWORKLOG,
            "%s: error running COBOL program %s, err = %d",
            progname, cobolfunc, A_call_err);

         if (!(prgdata->mailedmsgs & MSG_COBOLERROR)) {
            qmailmsg("%s: error running COBOL program %s, err = %d\n",
               progname, cobolfunc, A_call_err);
             prgdata->mailedmsgs |= MSG_COBOLERROR;
         }
      }

		sa.sa_handler = SIG_IGN;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGIO, &sa, (struct sigaction *)NULL);
		sigaction(SIGURG, &sa, (struct sigaction *)NULL);

		flags = 0;
		fcntl(s, F_SETOWN, 0);
		ioctl(s, FIOASYNC, &flags);

      if (prgdata->debuglog) {
         wescolog(db, "%d: status = %c%c\n", getpid(),
            clientmsg653.status[0], clientmsg653.status[1]);
         if (clientmsg653.status[0] != '0' || clientmsg653.status[1] != '0') {
             CobolToC(buf, clientmsg653.err.name, sizeof(buf), 
                 sizeof(clientmsg653.err.name));
             wescolog(db, "error name = %s\n", buf);
             CobolToC(buf, clientmsg653.err.description, sizeof(buf), 
                 sizeof(clientmsg653.err.description));
             wescolog(db, "error description = %s\n", buf);
        }
      }

      if (senddata(s, &clientmsg653, sizeof(clientmsg653), 0) < 
          sizeof(clientmsg653)) {
         wescolog(NETWORKLOG, "%s: %s disconnected unexpectedly", 
            progname, hostname);
         acu_shutdown(1);
         close(s);
         return;
      }
   }

   acu_shutdown(1);
   close(s);
}

int logit(RPTDATA *rpt)
{
    int fd;
    char buffer[256];

    if ((fd = open(logfile, O_CREAT|O_APPEND|O_WRONLY, READWRITE)) < 0) {
        wescolog(NETWORKLOG, "can't open %s", logfile);
        return -1;
    }

    chmod(logfile, READWRITE);

    sprintf(buffer, "%s, %s, %s\n", rpt->branch, rpt->sku,
        rpt->starttime);
    write(fd, buffer, strlen(buffer));

    free(rpt->starttime);

    close(fd);
    chmod(logfile, 0666);
}

int qmailmsg(char *fmt, ...)
{
    int fd;
    va_list ap;
    char buffer[256];
    char mfname[256];

    if (prgdata->mailto[0] == 0)
        return -1;

    sprintf(mfname, "%s.%d", MSQ_MAILFILE, getpid());
 
    if ((fd = open(mfname, O_CREAT|O_TRUNC|O_WRONLY, READWRITE)) < 0)
        return -1;

    chmod(mfname, READWRITE);

    strcpy(buffer, "maintserverQ has produced the following error:\n\n\n");
    write(fd, buffer, strlen(buffer));

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    strcat(buffer, "\n");
    write(fd, buffer, strlen(buffer));
    va_end(ap);

    close(fd);
}

void myshutdown(int status)
{
    if (prgdata->debuglog)
        wescolog(db, "received SIGUSR1.  shutting down\n");

    prgdata->status = MSQ_SHUTTINGDOWN;
    imdone = 1;
}

void myfuneral(int signo)
{
    int status;
    pid_t child_pid;

    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (prgdata->debuglog)
            wescolog(db, "%d: received SIGCHLD for %d\n", getpid(),
                child_pid);

        if (semop(semid, locksem, 2) < 0)
            perror(MQ_INI_SECTION);

        if ((WIFEXITED(status)) && (WEXITSTATUS(status) == SEM_EXIT)) {
            if (prgdata->debuglog)
                wescolog(db, "%d: this process did not increment counter\n",
                    getpid());
        }
        else {
            prgdata->numconnections--;

            if (prgdata->debuglog)
                wescolog(db, "%d: number of connections decreased to %d\n",
                    getpid(), prgdata->numconnections);
        }

        semctl(semid, 0, SETVAL, 0);
    }
}

void logshutdown(void)
{
   if (getpid() == prgdata->pid) {
      wescolog(NETWORKLOG, "%d: maintserverQ was terminated", getpid());
      if (prgdata->debuglog)
         wescolog(db, "%d: shutting down server\n", getpid());

      if (imdone) {
         prgdata->status = MSQ_TERMINATED;
         prgdata->pid = 0;
         shmdt(prgdata);
         umask(oldmask);
      }
   }
}

int setsignal(int signo)
{
   struct sigaction sigact;

   sigact.sa_handler = logsignal;
   sigemptyset(&sigact.sa_mask);
   sigaddset(&sigact.sa_mask, SIGABRT);
   sigaddset(&sigact.sa_mask, SIGALRM);
   sigaddset(&sigact.sa_mask, SIGBUS);
#if defined(SIGEMT)
   sigaddset(&sigact.sa_mask, SIGEMT);
#endif

   sigaddset(&sigact.sa_mask, SIGFPE);
   sigaddset(&sigact.sa_mask, SIGHUP);
   sigaddset(&sigact.sa_mask, SIGILL);
   sigaddset(&sigact.sa_mask, SIGPROF);
   sigaddset(&sigact.sa_mask, SIGSEGV);
   sigaddset(&sigact.sa_mask, SIGSYS);
   sigaddset(&sigact.sa_mask, SIGTERM);
   sigaddset(&sigact.sa_mask, SIGTRAP);
   sigaddset(&sigact.sa_mask, SIGUSR2);
   sigaddset(&sigact.sa_mask, SIGVTALRM);
   sigaddset(&sigact.sa_mask, SIGXCPU);
   sigaddset(&sigact.sa_mask, SIGXFSZ);
   sigact.sa_flags = SA_SIGINFO;
   return sigaction(signo, &sigact, NULL);
}

void logsignal(int signo, struct siginfo *logsig)
{
   wescolog(NETWORKLOG, "%d: received signal %s", getpid(),
      _sys_siglist[signo]);

   if (prgdata->debuglog) {
      wescolog(db, "%d: received signal %s\n", getpid(), _sys_siglist[signo]);
      if (logsig != NULL) {
         wescolog(db, "%d: siginfo dump:\n");
         wescolog(db, "%d: \tsi_signo = %d\n", getpid(), logsig->si_signo);
         wescolog(db, "%d: \tsi_errno = %d\n", getpid(), logsig->si_errno);
         wescolog(db, "%d: \tsi_code  = %d\n", getpid(), logsig->si_code);
         wescolog(db, "%d: \tsi_pid   = %d\n", getpid(), logsig->si_pid);
         wescolog(db, "%d: \tsi_uid   = %d\n", getpid(), logsig->si_uid);
      }
   }

   switch (signo) {
      case SIGTERM:
         exit(0);
      case SIGABRT:
      case SIGBUS:
#if defined(SIGEMT)
      case SIGEMT:
#endif

      case SIGFPE:
      case SIGILL:
      case SIGSEGV:
      case SIGSYS:
      case SIGTRAP:
      case SIGXCPU:
      case SIGXFSZ:
		case SIGPIPE:
         _exit(1);
   }
}

void sigio_handler(int signo)
{
	acu_cancel_all();
}
