/* Command to compile the program in your own directory
   gcc wdrecv.c -o wdreceive -D USEBUFFERLOG -D _GNU_SOURCE -lwesco-tcp -lwesco-user
*/

#include <stdlib.h>
#include <fcntl.h>

#ifndef _GNU_SOURCE
#include <sys/locking.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "receive.h"
#include "queue.h"

#ifndef MAXRECEIVES
#define MAXRECEIVES     25
#endif

#define DEBUGSERVER

int s, ls;

struct linger linger;
struct sockaddr_in myaddr, peeraddr;

int debugfile = 0;
int db;
char *tempQ;
char *progName = 0;
int socketdebug = 0;

#ifndef TESTSERVER
char *rcvVar = "WDOCRCVQ";
#else
char *rcvVar = "TWDOCRCVQ";
#endif

#ifdef USEBUFFERLOG
#define BUFFERLOG "/IMOSDATA/5/wdreceive.log"
#define OPTS "bfs"
int logbuffer = 0;
#else
#define OPTS "fs"
#endif

void AcceptConnections(void);
void Server(char *);
int GetFilename(char *, char *);
void sigusr1_handler(int);
void sigusr2_handler(int);

/*
 ************************************************************************
 *
 * Function:    Main
 *
 * Description: This function initializes the program.
 *
 * Parameters:  argc - The number of command line arguments
 *              argv - Pointers to the command line arguments
 *
 * Returns:     Exit code
 *
 ************************************************************************
*/
main(int argc, char *argv[])
{
   int retCode;
   int option;
   mode_t oldmask;
   struct sigaction newact;

   progName = argv[0];

   while ((option = getopt(argc, argv, OPTS)) != EOF) {
      switch (option) {
#ifdef USEBUFFERLOG
         case 'b':
            logbuffer = 1;
            break;
#endif
         case 'f':
            debugfile = 1;
            break;
         case 's':
            socketdebug = 1;
            break;
         default:
            exit(1);
      }
   }

   if (debugfile) {
      if ((db = creat("debug.wdr", READWRITE)) < 0) {
         perror(progName);
         debugfile = 0;
      }
   }

   /* get the environment variable WDOCRCVQ and append it to the paths */
   tempQ = getenv(rcvVar);
   if (tempQ == NULL) {
      wescolog(NETWORKLOG, 
         "%s: %s environment variable is not defined!  Server Terminated!",
         progName, rcvVar);
      return;
   }

   if (initaddress(&myaddr, "WDOCRECV", "tcp") < 0) {
      wescolog(NETWORKLOG,
         "%s: WDOCRECV not found in /etc/services.  Server terminated!", 
         progName);
      err_sys("error on service lookup");
   }

   if ((ls = getlistensocket(&myaddr, MAXRECEIVES)) < 0)
      err_sys("error creating listen socket");

   daemoninit();

   if (debugfile) {
      locallog(db, "debug logging is on\n");

#ifdef USEBUFFERLOG
      locallog(db, "buffer logging is %s\n", (logbuffer) ? "on" : "off");
#endif

      locallog(db, "socket level debug is %s\n", (socketdebug) ? "on" : "off");
   }

   /* set up SIGCHLD handler */
   newact.sa_handler = SIG_IGN;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGUSR1);
   sigaddset(&newact.sa_mask, SIGUSR2);
   newact.sa_flags = (SA_NOCLDSTOP|SA_NOCLDWAIT);
   sigaction(SIGCHLD, &newact, NULL);

   /* set up SIGUSR1 handler */
   newact.sa_handler = sigusr1_handler;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGUSR2);
   sigaddset(&newact.sa_mask, SIGCHLD);
   newact.sa_flags = 0;
   sigaction(SIGUSR1, &newact, NULL);

   /* set up SIGUSR2 handler */
   newact.sa_handler = sigusr2_handler;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGUSR1);
   sigaddset(&newact.sa_mask, SIGCHLD);
   newact.sa_flags = 0;
   sigaction(SIGUSR2, &newact, NULL);

   AcceptConnections();

   return 1;
}

/*
 ************************************************************************
 *
 * Function:    AcceptConnections
 *
 * Description: This function is an infinite loop that accepts a 
 *              conversation.  When accepted, the Server function
 *              is called to perform the actual service.
 *
 * Parameters:  None
 *
 * Returns:     Nothing
 *
 ************************************************************************
*/
void AcceptConnections(void)
{
   size_t addrLen;
   int serrno;
	int lock_fd;
   char *hostname;
   struct hostent *hp;

   int lb;

   /* loop forever to accept a conversation from a client */
   for (;;) {
      addrLen = sizeof(struct sockaddr_in);
      if ((s = accept(ls, (struct sockaddr *)&peeraddr, (socklen_t *) &addrLen)) < 0) {
         serrno = errno;
         wescolog(NETWORKLOG, "%s: error on accept(); errno = %d\n",
            progName, serrno);

         if (debugfile)
            locallog(db, "error on accept(); errno = %d\n", serrno);

         close(ls);
         sleep(60);

         if (debugfile)
            locallog(db, "calling getlistensocket()\n");

         while ((ls = getlistensocket(&myaddr, MAXRECEIVES)) < 0) {
            serrno = errno;
            wescolog(NETWORKLOG,
               "%s: unable to create listen socket, errno = %d\n",
               progName, serrno);

            if (debugfile) {
               locallog(db, "error on getlistensocket(); errno = %d\n",
                   serrno);
               locallog(db, "calling getlistensocket()\n");
            }
         }
      }
      else {
			if (socketdebug)
				setsocketdebug(s, 1);

			if ((lock_fd = open(WESCOM_LOCK_FILE, O_RDONLY)) >= 0) {
				close(s);
				close(lock_fd);
				continue;
			}

         /*
            Fork the program.  The child process will call the Server
            function.  The parent process will resume listening for
            more conversations.  If -1 is returned an error has occurred,
            exit the program.
         */
         switch (fork()) {
            case -1:
               serrno = errno;
               wescolog(NETWORKLOG, 
                  "%s: could not fork daemon; errno = %d\n", progName,
                  serrno);

               if (debugfile)
                  locallog(db, "error on fork(); errno = %d\n", serrno);

               close(s);
               break;
            case 0:
               close(ls);

               if (AuthorizeConnection(s, &hp, &peeraddr)) {
                  #ifdef USEBUFFERLOG
                        if (logbuffer) {
                           if ((lb = open(BUFFERLOG, O_CREAT|O_APPEND|O_WRONLY, 0666)) > -1) {
                              locallog(lb, "*** Accepted connection from %s i/p %s ***\n",
                                 hp->h_name, inet_ntoa(peeraddr.sin_addr));
                              close(lb);
                           }
                        }
                  #endif
                  Server(hp->h_name);
               }
               else {
                  hostname = inet_ntoa(peeraddr.sin_addr);
                  wescolog(NETWORKLOG, 
                     "%s: unknown host at address %s! Security disconnect.\n", 
                     progName, hostname);

                  if (debugfile)
                     locallog(db, "unknown host at %s! Disconnecting!\n",
                         hostname);
               }

               exit(0);
               break;
            default:
               if (debugfile)
                  locallog(db, "resume accepting connections\n");

               close(s);
         }
      }
   }
}

/*
 ************************************************************************
 *
 * Function:    Server
 *
 * Description: This function receives the files from the client
 *              and places it in the proper directory.
 *
 * Parameters:  None
 *
 * Returns:     Nothing
 *
 ************************************************************************
*/
void Server(char *hostName)
{
   char buffer[1024];
   char transaction[TRANSFILERECLEN];
   char fullPath[255], filename[FILENAMELENGTH], newFilename[FILENAMELENGTH]; 
   char dataFilename[FILENAMELENGTH], QPath[FILENAMELENGTH];
   char *OK = "OK", *EOFMarker;
   int totalBytes = 0, bytesReceived = 0, pos = 0, done = FALSE;
   int outFile, serrno;
   int lb;
   int packet;

   if (debugfile)
      locallog(db, "processing connection from %s\n", hostName);

   while (1) {
      /* clear the memory buffers */
      memset(fullPath, 0, sizeof(fullPath));
      memset(filename, 0, sizeof(filename));
      memset(transaction, 0, sizeof(transaction));
      memset(QPath, 0, sizeof(QPath));
      done = FALSE;

      strcpy(QPath, tempQ);
      pos = strlen(QPath) - 1;
      if (QPath[pos] != '/')
         QPath[pos+1] = '/';
      strcpy(fullPath, QPath);

      /*
         Get the transaction from the client.  If an error occurs close the
         socket and return to the accept.
      */
      if (debugfile)
         locallog(db, "receiving transaction data\n");

      bytesReceived = ReceiveData(s, buffer, 1024, 0);
      if (bytesReceived < 1)
         break;

#ifdef USEBUFFERLOG
      if (logbuffer) {
         if ((lb = open(BUFFERLOG, O_CREAT|O_APPEND|O_WRONLY, 0666)) > -1) {
            locallog(lb, "*** Transaction  ***\n");
            write(lb, buffer, sizeof(buffer));
            write(lb, "\n", 1);
            locallog(lb, "*** IP address: %s  ***\n", inet_ntoa(peeraddr.sin_addr));
            close(lb);
         }
      }
#endif

      /* create a filename for the file just received */
      strncat(transaction, buffer, TRANSFILERECLEN);
      strncat(fullPath, transaction, TRANSACTIONSIZE);
      strcat(fullPath, "/");
      strcpy(filename, fullPath);
      pos = strlen(filename);
      if (!GetFilename(QPath, filename+pos)) {
         close(s);
         return;
      }

      if (debugfile)
         locallog(db, "transaction type is %s\n", transaction);

      strcpy(dataFilename, filename);
      strcat(filename, ".X");
      strcat(dataFilename, ".dat");        
 
      if (debugfile) {
         locallog(db, "Queue filename -> %s\n", filename);
         locallog(db, "Data  filename -> %s\n", dataFilename);
      }

      /*
         Receive the transaction file from the client.  The
         transaction file will have an initial extension of
         .X; when the file and data file is received the file
         will be given a .Q extension.
      */
      if ((outFile = open(filename, O_CREAT|O_WRONLY)) < 0) {
         serrno = errno;
         wescolog(NETWORKLOG, "%s: can not open file %s; errno = %d\n",
            progName, filename, serrno);

         if (debugfile)
            locallog(db, "can not open file %s; errno = %d\n",
               filename, serrno);

         return;
      }

      write(outFile, transaction, sizeof(transaction)-1);
      close(outFile);
      chmod(filename, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

      if (debugfile) {
         locallog(db, "file %s was successfully created\n", filename);
         locallog(db, "pid %d: sending acknowledgment\n", getpid());
		}

      /* send OK to the client */
      if (send(s, OK, 2, 0) != 2) {
         serrno = errno;
         wescolog(NETWORKLOG, 
            "%s: error sending acknowledgement; errno = %d\n",
            progName, serrno);

         if (debugfile)
            locallog(db, "error on send(); errno = %d\n", serrno);

         close(s);        
         return;
      }

      if (debugfile)
         locallog(db, "acknowledgment sent successfully\n");

      /*
         Get the data file from the client if there is one.  The
         last letter in the transaction buffer is F if there is
         a file or N for no file.
      */
      if (transaction[strlen(transaction)-2] == 'F') {
         if (debugfile)
            locallog(db, "receiving data file %s\n", dataFilename);

         if ((outFile = open(dataFilename, O_CREAT|O_WRONLY)) < 0) {
            serrno = errno;
            wescolog(NETWORKLOG, "%s: can not create file %s; errno = %d\n",
               progName, dataFilename, serrno);

            if (debugfile)
               locallog(db, "can not create file %s; errno = %d\n",
                  dataFilename, serrno);

            return;
         }

         packet = 0;
         while (!done) {
            memset(buffer, 0, sizeof(buffer));
            bytesReceived = ReceiveData(s, buffer, sizeof(buffer), 0);
            if (bytesReceived < 1) {
               close(s);
               return;
            }

#ifdef USEBUFFERLOG
            if (logbuffer) {
               if ((lb = open(BUFFERLOG, O_CREAT|O_APPEND|O_WRONLY, 0666)) > -1)
               {
                  locallog(lb, "*** %s - Packet %d ***\n",
                     dataFilename, packet);
                  packet++;
                  write(lb, buffer, sizeof(buffer));
                  write(lb, "\n", 1);
                  close(lb);
               }
            }
#endif

            for (EOFMarker = buffer, pos = 0; pos < 1024 && *EOFMarker != 127;
               pos++, EOFMarker++);

            if (pos < 1024)
               done = TRUE;
            else
               pos = 1024;

            if (debugfile) {
               locallog(db, "%d bytes written.\n", pos);
            }

            write(outFile, buffer, pos);
         }
      }
      else
         if (debugfile)
            locallog(db, "no associated data file\n");

      chmod(dataFilename, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
      close(outFile);

      /* rename the .X file */
      strcpy(newFilename, filename);
      newFilename[strlen(newFilename)-1] = 'Q';
      rename(filename, newFilename);

      if (debugfile) {
         locallog(db, "renamed %s to %s\n", newFilename, filename);
         locallog(db, "sending acknowledgment\n");
      }

      /* send OK to the client */
      if (send(s, OK, 2, 0) != 2) {
         serrno = errno;
         wescolog(NETWORKLOG, "%s: connection terminated by remote\n",
            progName);

         if (debugfile)
            locallog(db, "error on send(); errno = %d\n", serrno);

         close(s); 
         return;
      }
 
      if (debugfile)
         locallog(db, "acknowledgment sent successfully\n");
   }
}

/*
 ************************************************************************
 *
 * Function: GetFilename
 *
 * Description: This function creates the filename for the .Q and .dat
 *              files.
 *
 * Parameters:  QPath        - Path to the Queue directory structure
 *              destFilename - Filename to create
 *
 * Returns:     TRUE if filename was created, else FALSE
 *
 ************************************************************************
*/
int GetFilename(char *QPath, char *destFilename)
{   
   int pos, nextNumber, seqFile, serrno;
   char seqFilename[FILENAMELENGTH], seqNumber[7];
   struct flock seqLock;

   /* initialize the filename and memory buffers */
   memset(seqFilename, 0, sizeof(seqFilename));
   memset(seqNumber, 0, sizeof(seqNumber));

   strcpy(seqFilename, QPath);
   pos = strlen(seqFilename) - 1;
   if (seqFilename[pos] != '/') {
      seqFilename[pos+1] = '/';
      seqFilename[pos+2] = 0;
   }
   strcat(seqFilename, "seqnum");

   /* open the file */
   if ((seqFile = open(seqFilename, O_RDWR)) < 0) {
      serrno = errno;
      wescolog(NETWORKLOG, "%s: can't open sequence file - %s",
         progName, seqFilename);

      if (debugfile)
         locallog(db, "error opening %s; errno = %d\n", seqFilename, serrno);

      return FALSE;
   }

   /* 
      Lock the file to ensure that another process can not use the same
      number that this process is using.
   */
   seqLock.l_type = F_WRLCK;
   seqLock.l_whence = 0;
   seqLock.l_start = 0;
   seqLock.l_len = 0;
   if (fcntl(seqFile, F_SETLKW, &seqLock)) {
      serrno = errno;
      wescolog(NETWORKLOG, "%s: can't lock sequence file - %s",
         progName, seqFilename);

      if (debugfile)
         locallog(db, "can not lock %s; errno = %d\n", seqFilename, serrno);

      return FALSE;
   }

   /*
      Read in the current value in the file "seqnum".  Increment it and
      check to see if it is greater than 999999.  If it is, set the
      number back to 000000.  Write the current number to disk.  Unlock
      the file.
   */
   if (read(seqFile, seqNumber, 6) < 6) {
      serrno = errno;
      wescolog(NETWORKLOG, "%s: error reading %s; errno = %d\n", progName,
         seqFilename, serrno);

      if (debugfile)
         locallog(db, "error reading %s; errno = %d\n", seqFilename, serrno);

      return FALSE;
   }

   seqNumber[6] = 0;
   strcat(destFilename, seqNumber);
   sscanf(seqNumber, "%6uld", &nextNumber);
   if ((++nextNumber) > 999999)
      nextNumber = 0;
   sprintf(seqNumber, "%000006uld", nextNumber);
   lseek(seqFile, 0, 0);

   if (write(seqFile, seqNumber, 6) < 6) {
      serrno = errno;
      wescolog(NETWORKLOG, "%s: error writing to %s; errno = %d\n", progName,
         seqFilename, serrno);

      if (debugfile)
         locallog(db, "error writing to %s; errno = %d\n", seqFilename, serrno);

      return FALSE;
   }

   seqLock.l_type = F_UNLCK;
   fcntl(seqFile, F_SETLK, &seqLock);

   close(seqFile);

   return TRUE;
}

void sigusr1_handler(int status)
{
   if ((db = creat("debug.wdr", READWRITE)) > -1)
      debugfile = 1;
}

void sigusr2_handler(int status)
{
   close(db);
   debugfile = 0;
}
