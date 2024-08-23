
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>

#ifdef _GNU_SOURCE
#include <signal.h>
#else
#include <siginfo.h>
#endif


#include "maint.h"
#include "newmaint.h"
#include "maintc.h"
#include "sharemem.h"

void doloop(void);
socket_t sopen(struct sockaddr_in *);
int mailmsg(char *, ...);
void soundalarm(int);
int transmitqueuefile(char *, short, struct sockaddr_in *, char);
int getpeeraddress(struct sockaddr_in *);
void terminate(int);
void cleanup(void);
void logshutdown(void);
int setsignal(int);
void logsignal(int, siginfo_t *);
void set_socket_timeouts(int s, int snd_to, int rcv_to);

#ifndef WESCOMINI
#define WESCOMINI     "/IMOSDATA/2/wescom.ini"
#endif

#define PACKET_PROCESSING 'P'
#define RECORD_PROCESSING 'R'

MAINTTRANSDATA *prgdata;       /* global memory pointer for data */
int shmid;
int debuglog;
int debugsocket;
volatile int alarmed;
char version[VERSIONLENGTH];
struct sigaction alrmact;
struct sigaction usr1act;
sigset_t sigmask;
short retry = 0;
int db, debugFile;

#define CONNECTION_CLEANUP(sockfd,bufferfd) \
	{ \
		shutdown(sockfd, 2); \
		close(sockfd); \
		close(bufferfd); \
	}

int main(int argc, char *argv[])
{
   char logname[FILENAMELENGTH];  /* debug log filename             */
   int option;
   bool_t flag = FALSE;
   mode_t oldmask;

   oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

   sigemptyset(&sigmask);
   sigaddset(&sigmask, SIGTERM);
   sigaddset(&sigmask, SIGHUP);
   sigaddset(&sigmask, SIGINT);
   sigaddset(&sigmask, SIGQUIT);
   sigprocmask(SIG_BLOCK, &sigmask, NULL);

   GetPrivateProfileString("IQxmit", "debugfile", logname,
      sizeof(logname), "debug.mq", WESCOMINI);

   if ((debuglog = creat(logname, READWRITE)) < 0)
      err_sys("can not create debug log\n");

   /* parse the command line options */
   while ((option = getopt(argc, argv, "dfs")) != EOF) {
      switch (option) {
         case 'f':
         case 'd': /* provided for backwards compatibilty */
            flag = TRUE;
            wescolog(debuglog, "log created\n");
            break;
         case 's':
            debugsocket = TRUE;
            break;
         default:
            printf("usage: IQxmit -fs\n\n");
            exit(1);
      }
   }

   if (flag)
      wescolog(debuglog, "initializing application\n");

   /* get shared memory and read in initial values */
   prgdata = (MAINTTRANSDATA *)GetSharedMemory(&shmid, SHM_NEWMAINT,
      sizeof(MAINTTRANSDATA), IPC_CREAT | READWRITE);
   if (!prgdata)
      err_sys("error initializing shared memory");

   if (prgdata->pidtx)
      if (kill(prgdata->pidtx, SIGWINCH) > -1)
         err_quit("can not have more than one instance of IQxmit\n");

   prgdata->txstatus = MNT_INITIALIZING;
   prgdata->timeout = GetPrivateProfileInt("IQxmit", "timeout", 30,
      WESCOMINI);
   prgdata->rejections = 0;
   prgdata->mailedmsgs = 0;
   prgdata->wesdocdown = FALSE;
   prgdata->deletefiles = TRUE;
   prgdata->debugtx = flag;
   GetPrivateProfileString("IQ", "mail-to", prgdata->mailto,
      sizeof(prgdata->mailto), "root", WESCOMINI);
   prgdata->maxretries = GetPrivateProfileInt("IQxmit", "maxretries", 5,
      WESCOMINI);
 
   if (prgdata->debugtx)
      wescolog(debuglog, "shared memory initialization complete\n");

   daemoninit(argv[0]);
   prgdata->pidtx = getpid();

   setsignal(SIGABRT);
   setsignal(SIGALRM);
   setsignal(SIGBUS);
#if defined(SIGEMT)
   setsignal(SIGEMT);
#endif

   setsignal(SIGFPE);
   setsignal(SIGHUP);
   setsignal(SIGILL);
   setsignal(SIGINT);
   setsignal(SIGIO);
   setsignal(SIGPOLL);
   setsignal(SIGPROF);
   setsignal(SIGQUIT);
   setsignal(SIGPIPE);
   setsignal(SIGSEGV);
   setsignal(SIGSYS);
   setsignal(SIGTERM);
   setsignal(SIGTRAP);
   setsignal(SIGVTALRM);
   setsignal(SIGXCPU);
   setsignal(SIGXFSZ);
   atexit(cleanup);
   atexit(logshutdown);

   doloop();

   if (prgdata->debugtx) {
      wescolog(debuglog, "shutting down\n");
      close(debuglog);
   }

   umask(oldmask);

   return 0;
}

void doloop(void)
{
   short count;
   int fd;
   short blksize;
   int bytes;
   struct stat fs;
   u_int32_t rc = 0;
   short retries = 0;
   struct sockaddr_in peeraddr;
   char queuedir[FILENAMELENGTH];
   char qprefix[FILENAMELENGTH];
   char name[FILENAMELENGTH];
   char filename[FILENAMELENGTH];
   char *temp;
   bool_t save_as_error = FALSE;
   char process_type;
   int retval;

   /* set up SIGALRM handler */
   alrmact.sa_handler = soundalarm;
   sigemptyset(&alrmact.sa_mask);
   sigaddset(&alrmact.sa_mask, SIGUSR1);
   alrmact.sa_flags = 0;
   sigaction(SIGALRM, &alrmact, NULL);

   /* set up SIGUSR1 handler */
   usr1act.sa_handler = terminate;
   sigemptyset(&usr1act.sa_mask);
   sigaddset(&usr1act.sa_mask, SIGALRM);
   usr1act.sa_flags = 0;
   sigaction(SIGUSR1, &usr1act, NULL);

   /* read INI file */
   GetPrivateProfileString("IQ", "version", version, sizeof(version),
      "06.05.03", WESCOMINI);
   GetPrivateProfileString("IQ", "queuedir", queuedir, sizeof(queuedir),
      "/IMOSDATA/2/QUEUES", WESCOMINI);
   GetPrivateProfileString("IQ", "queueprefix", qprefix, sizeof(qprefix),
      "queue", WESCOMINI);

   if (prgdata->debugtx)
      wescolog(debuglog, "the application is running version %s\n",
         version);

   if (prgdata->debugtx)
      wescolog(debuglog, "queuedir = %s\n", queuedir);

   /* remove trailing '/' if necessary */
   if (queuedir[strlen(queuedir) - 1] == '/')
      queuedir[strlen(queuedir) - 1] = 0;

   /* get the network address for wesdoc */
   if (getpeeraddress(&peeraddr)) {
      if (prgdata->debugtx)
         wescolog(debuglog, "error getting wesdoc address\n");
      return;
   }

   /* we're off and running */
   prgdata->txstatus = MNT_RUNNING;

   while (1) {
      sleep(1);  /* give everyone else a chance to run */

      if (prgdata->txstatus == MNT_SHUTTINGDOWN)
         return;

      /* reset the mailedmsgs flag at the top of the hour */
      if ((time(NULL) % 3600) < 2)
         prgdata->mailedmsgs = 0;

      if (!strcmp(filename, ""))
         findnextfile(queuedir, filename);

      /* if no file was found, loop and look again */
      if (!strcmp(filename, "")) {
         sleep(5);
         continue;
      }

      if (stat(filename, &fs) >= 0) {
         if (fs.st_size < sizeof(LINEITEM)) {
            unlink(filename);
            continue;
         }

         if (prgdata->debugtx)
            wescolog(debuglog, "transmit %s; %d records\n", filename,
               fs.st_size / sizeof(LINEITEM));

         if (retries == prgdata->maxretries) {
            process_type = RECORD_PROCESSING;
            save_as_error = TRUE;
         }
         else
            process_type = PACKET_PROCESSING;

         /* transmit the queue file */
         retval = transmitqueuefile(filename, 
            fs.st_size / sizeof(LINEITEM), &peeraddr, process_type);
         if (retval != 0) {      /* error transmitting the file */
            if (retval >= 20)
              retries++;
            else
              prgdata->wesdocdown = TRUE;

            sleep(5);
            continue;
         }
         else {    /* file transmitted successfully */
            retries = 0;
            if (prgdata->wesdocdown) {
               prgdata->wesdocdown = FALSE;
               kill(prgdata->pidqueue, SIGUSR2);
            }
         }
      }

      if (save_as_error) {
         save_as_error = FALSE;
         if ((temp = strstr(filename, "towesdoc")) != NULL) {
            sprintf(name, "%s/error%s", queuedir, temp);
            rename(filename, name);
         }
      }
      else if (prgdata->deletefiles)
         unlink(filename);
      else {
         if ((temp = strstr(filename, "towesdoc")) != NULL) {
            sprintf(name, "%s/sent%s", queuedir, temp);
            rename(filename, name);
         }
      }

      strcpy(filename, "");
   }
}

/*
 ******************************************************************************
 *
 * Name:        sopen
 *
 * Description: Creates a socket and connects to WESDOC.
 *
 * Parameters:  peer - Pointer to structure containing the peer's address.
 *
 * Returns:     socket descriptor, -1 on error
 *
 ******************************************************************************
*/
socket_t sopen(struct sockaddr_in *peer)
{
   socket_t s;    /* socket descriptor */

   /* create the socket */
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      if (!(prgdata->mailedmsgs & MSG_SOCKET)) {
         mailmsg("Error creating socket: %d - %s\n", errno, 
            strerror(errno));
         prgdata->mailedmsgs |= MSG_SOCKET;

         if (prgdata->debugtx)
            wescolog(debuglog, "Error creating socket: %d - %s\n", errno,
               strerror(errno));
      }
      return -1;
   }

   /* connect to wesdoc */
   alarm(prgdata->timeout);    /* set the alarm for timeout signal */
   if (connect(s, (struct sockaddr *)peer, sizeof(struct sockaddr_in)) < 0) {
      if (!(prgdata->mailedmsgs & MSG_CONNECT)) {
         mailmsg("Error connecting to wesdoc: %d - %s\n", errno, 
            strerror(errno));
         prgdata->mailedmsgs |= MSG_CONNECT;
      }

      if (prgdata->debugtx)
         wescolog(debuglog, "Error connecting to wesdoc: %d - %s\n", 
            errno, strerror(errno));

      close(s);
      s = -1;
   }
   alarm(0);                  /* reset the alarm */

   if (prgdata->debugtx)
      wescolog(debuglog, "connected to wesdoc\n");

   return s;
}

int mailmsg(char *fmt, ...)
{
   va_list ap;
   char buffer[512];

   if (prgdata->mailto[0] == 0)
      return -1;

   sprintf(buffer, "%s\n\n\n", "IQxmit has produced the following error:");

   va_start(ap, fmt);
   vsprintf(buffer+strlen(buffer), fmt, ap);
   strcat(buffer, "\n");
   va_end(ap);

   mailmessage(prgdata->mailto, buffer);

   if (prgdata->debugtx)
      wescolog(debuglog, "mail sent to %s\n", prgdata->mailto);

   return 0;
}

void soundalarm(int status)
{
   alarmed = TRUE;
}

int transmitqueuefile(char *filename, short blocksize,
      struct sockaddr_in *peer, char process_type)
{
   int buffd;
   socket_t s;
   int count;
   bool_t waitforack = TRUE;
   char ack;
   short record_number;
   bool_t transmitted = TRUE;
   MNTMESSAGE message;
   char status[3];
   size_t record_size;
   int error_code;
   char *filebase;
   bool_t done = TRUE;
   int error_status;

   memset(status, 0, sizeof(status));

   if ((buffd = open(filename, O_RDONLY)) < 0)
      return -1;

   /* the IQcontrol may be in this loop for a while */
   /* check to see if the message flags should be   */
   /* reset.                                        */
   if ((time(NULL) % 3600) < 2)
      prgdata->mailedmsgs = 0;

   /* open the socket */
   if ((s = sopen(peer)) < 0) {
      close(buffd);
      return -1;
   }

	set_socket_timeouts(s, 120, 120);

   if (prgdata->debugtx)
      wescolog(debuglog, "transmitting version information\n");

   /* send the version */
   if (senddata(s, version, sizeof(version), 0) < sizeof(version)) {
		CONNECTION_CLEANUP(s,buffd)
      return -1;
   }

   if (prgdata->debugtx)
      wescolog(debuglog, "waiting for acknowledgement\n");

   /* get reply from remote */
   if (recvdata(s, &ack, sizeof(ack), 0) < 0) {
		CONNECTION_CLEANUP(s,buffd)
      return -1;
   }

   if (!ack) {
      if (!(prgdata->mailedmsgs & MSG_BADVERSION)) {
         mailmsg("version %s is not supported by wesdoc\n",
            version);
         prgdata->mailedmsgs |= MSG_BADVERSION;
      }

      if (prgdata->debugtx)
         wescolog(debuglog, "version is not supported by wesdoc\n");

		CONNECTION_CLEANUP(s,buffd)
      return -1;
   }

   if (prgdata->debugtx)
      wescolog(debuglog, "received acknowledgement\n");

   done = FALSE;
   record_number = 0;

   if (prgdata->debugtx)
      wescolog(debuglog, "initializing data packet\n");

   /* initialize the message */
   memset(&message, 32, sizeof(message));
   message.process_type = process_type;
   record_size = sizeof(message.lineitem[record_number]);

   do {
      if (readn(buffd, &message.lineitem[record_number], record_size) < 0) {
         if (prgdata->debugtx)
            wescolog(debuglog, "error reading file - %s\n", strerror(errno));

			CONNECTION_CLEANUP(s,buffd)
         return -1;
      }

      record_number++;
      if (record_number == 50 ||
          record_number == blocksize) {
          if (prgdata->debugtx)
             wescolog(debuglog, "sending packet size of %d\n", record_number);

         /* transmit index of last element in array */
         if (senddata(s, &record_number, sizeof(record_number), 0) <
             sizeof(record_number)) {
            if (prgdata->debugtx)
               wescolog(debuglog, "error sending data - %s\n", strerror(errno));

				CONNECTION_CLEANUP(s,buffd)
            return -1;
          }

         if (prgdata->debugtx)
            wescolog(debuglog, "transmitting data packet\n");

         /* transmit the data packet */
         if (senddata(s, &message, sizeof(message), 0) < sizeof(message)) {
            if (prgdata->debugtx)
               wescolog(debuglog, "error sending data - %s\n", strerror(errno));

				CONNECTION_CLEANUP(s,buffd)
            return -1;
         }

         if (prgdata->debugtx)
            wescolog(debuglog, "waiting for reply from wesdoc\n");

         /* wait for status */
         alarm(prgdata->timeout*4);
         if (recvdata(s, &message, sizeof(message), 0) < sizeof(message)) {
            if (prgdata->debugtx)
               wescolog(debuglog, "error receiving data - %s\n",
                  strerror(errno));

            alarm(0);
				CONNECTION_CLEANUP(s,buffd)
            return -1;
         }
         else
            alarm(0);

         /* check the return status */
         memset(status, 0, sizeof(status));
         memcpy(status, message.status, sizeof(message.status));
         error_status = atoi(status);

         if (prgdata->debugtx)
            wescolog(debuglog, "received status of %d\n", error_status);

         if (error_status != 0) {
            record_number = 0;
            senddata(s, &record_number, sizeof(record_number), 0);
				CONNECTION_CLEANUP(s,buffd)
            return error_status;
         }

         /* did it transmit all of the records? */
         if (record_number != blocksize) {
            if (prgdata->debugtx)
               wescolog(debuglog, "preparing to transmit more data\n");

            blocksize -= record_number;
            record_number = 0;
            memset(message.lineitem, 32, sizeof(message.lineitem));
         }
         else {
            done = TRUE;
            if (prgdata->debugtx)
               wescolog(debuglog, "completed transmitting data\n");
         }
      }
   } while (!done);

   record_number = 0;
   if (senddata(s, &record_number, sizeof(record_number), 0) <
       sizeof(record_number)) {
		CONNECTION_CLEANUP(s,buffd)
      return -1;
   }

	CONNECTION_CLEANUP(s,buffd)

   alarm(0);

   return 0;
}

void terminate(int status)
{
   prgdata->txstatus = MNT_SHUTTINGDOWN;
}

void cleanup(void)
{
   prgdata->wesdocdown = TRUE;
   prgdata->txstatus = MNT_TERMINATED;
   prgdata->pidtx = (pid_t)0;
   shmdt(prgdata);
}

int getpeeraddress(struct sockaddr_in *peer)
{
   struct hostent *hp;
   struct servent *sp;
   char service[25];
   char protocol[25];

   GetPrivateProfileString("IQ", "service", service, sizeof(service),
      "WDOCUPDQ", WESCOMINI);
   GetPrivateProfileString("IQ", "protocol", protocol, 
      sizeof(protocol), "tcp", WESCOMINI);

   if ((hp = ws_gethostbyname("wesdoc")) == NULL) {
      if (!(prgdata->mailedmsgs & MSG_NOTHOST)) {
         mailmsg("wesdoc not in /etc/hosts file\n");
         prgdata->mailedmsgs |= MSG_NOTHOST;

         if (prgdata->debugtx)
            wescolog(debuglog, "wesdoc not in /etc/hosts file\n");
      }
      return -1;
   }

   if ((sp = getservbyname(service, protocol)) == NULL) {
      if (!(prgdata->mailedmsgs & MSG_NOTSERVICE)) {
         mailmsg("%s/%s not in /etc/services\n", service, protocol);
         prgdata->mailedmsgs |= MSG_NOTSERVICE;

         if (prgdata->debugtx)
            wescolog(debuglog, "%s/%s not in /etc/services\n", service,
               protocol);
      } 
      return -1;
   }

   peer->sin_family = AF_INET;
   peer->sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   peer->sin_port = sp->s_port;

   return 0;
}

int findnextfile(char *thedirname, char *filename)
{
   DIR *dir;
   struct dirent *entry;
   int error_number;
   char *period;
   USEFILE queuefile;
   UTIME filetime;

   strcpy(queuefile.filename, "");
   queuefile.filetime.seconds = UINT_MAX;
   queuefile.filetime.hundreds = UINT_MAX;

   /* open queue directory for reading */
   if ((dir = opendir(thedirname)) == NULL) {
      if (!(prgdata->mailedmsgs & MSG_BADQUEUEDIR)) {
         error_number = errno;
         mailmsg("Error opening directory: %s %d - %s\n", thedirname,
            error_number, strerror(error_number));
         prgdata->mailedmsgs |= MSG_BADQUEUEDIR;
      }
      
      if (prgdata->debugtx)
         wescolog(debuglog, "Error opening directory: %s %d - %s\n",
             thedirname, error_number, strerror(error_number));

      return -1;
   }

   /* loop for every entry in the directory */
   while ((entry = readdir(dir)) != NULL) {
      if (file_pattern_match(entry->d_name, "towesdoc.*")) {
         if ((period = strchr(entry->d_name, '.')) != NULL) {
            filetime.seconds = strtoul(period + 1, &period, 10);
            filetime.hundreds = strtoul(period + 1, &period, 10);

            if ((filetime.seconds < queuefile.filetime.seconds) ||
               ((filetime.seconds == queuefile.filetime.seconds) &&
                (filetime.hundreds < queuefile.filetime.hundreds))) {
               queuefile.filetime = filetime;
               strcpy(queuefile.filename, thedirname);
               strcat(queuefile.filename, "/");
               strcat(queuefile.filename, entry->d_name);
            }
         }
      }
   }

   strcpy(filename, queuefile.filename);
   closedir(dir);

   return 0;
}

void logshutdown(void)
{
   wescolog(NETWORKLOG, "%d: IQxmit was terminated", getpid());

   if (prgdata->debugtx) {
      wescolog(debuglog, "%d: processing normal shutdown\n", getpid());
   }
}

int setsignal(int signo)
{
   struct sigaction sigact;

   sigact.sa_handler = (void (*)(int))logsignal;
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
   sigaddset(&sigact.sa_mask, SIGINT);
   sigaddset(&sigact.sa_mask, SIGIO);
   sigaddset(&sigact.sa_mask, SIGPOLL);
   sigaddset(&sigact.sa_mask, SIGPROF);
   sigaddset(&sigact.sa_mask, SIGQUIT);
   sigaddset(&sigact.sa_mask, SIGPIPE);
   sigaddset(&sigact.sa_mask, SIGSEGV);
   sigaddset(&sigact.sa_mask, SIGSYS);
   sigaddset(&sigact.sa_mask, SIGTERM);
   sigaddset(&sigact.sa_mask, SIGTRAP);
   sigaddset(&sigact.sa_mask, SIGVTALRM);
   sigaddset(&sigact.sa_mask, SIGXCPU);
   sigaddset(&sigact.sa_mask, SIGXFSZ);
   sigact.sa_flags = SA_SIGINFO;
   return sigaction(signo, &sigact, NULL);
}

void set_socket_timeouts(int s, int snd_to, int rcv_to)
{
	struct timeval tv;

	/* set the SO_SNDTIMEO option */
	tv.tv_sec = snd_to;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	/* set the SO_RCVTIMEO option */
	tv.tv_sec = rcv_to;
	tv.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void logsignal(int signo, siginfo_t *logsig)
{
   wescolog(NETWORKLOG, "%d: received signal %s", getpid(),
      _sys_siglist[signo]);

   if (prgdata->debugtx) {
      wescolog(debuglog, "%d: received signal %s\n", getpid(),
         _sys_siglist[signo]);
      if (logsig != NULL) {
         wescolog(debuglog, "%d: siginfo dump:\n");
         wescolog(debuglog, "%d: \tsi_signo = %d\n", getpid(),
            logsig->si_signo);
         wescolog(debuglog, "%d: \tsi_errno = %d\n", getpid(),
            logsig->si_errno);
         wescolog(debuglog, "%d: \tsi_code  = %d\n", getpid(), logsig->si_code);
         wescolog(debuglog, "%d: \tsi_pid   = %d\n", getpid(), logsig->si_pid);
         wescolog(debuglog, "%d: \tsi_uid   = %d\n", getpid(), logsig->si_uid);
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
         _exit(1);
   }
}
