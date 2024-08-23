
#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1997 WESCO, Inc.\nAll rights reserved.\n";
#endif

/*
 ******************************************************************************
 * History:
 * 09/29/06 mjb 06142 - Recompiled for bonded inventory.
 * 11/13/06 mjb 16067 - Modified SIGCHLD handling to properly clean up defunct
 *                      child processes.
 ******************************************************************************
*/
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "rsvservr.h"
#include "part.h"

char *inifile = "/IMOSDATA/2/wescom.ini";

/*
 ****************************************************************************
 *
 *  Function:    main
 *
 *  Description: This function initializes the server.
 * 
 *  Parameters:  argc     - The number of command line arguments
 *               argv     - Pointers to the command line arguments
 *
 *  Returns:     Exit code
 *
 ****************************************************************************
*/
/* ARGSUSED */
main(int argc, char *argv[])
{
   int option;
   extern char *optarg;

   progName = argv[0];

   while ((option = getopt(argc, argv, "?adfi:s")) != EOF) {
      switch (option) {
         case '?':
            exit(0);
            break;
         case 'a':
            debugCobol = debugFile = debugSocket = TRUE;
            break;
         case 'd':
            debugCobol = TRUE;
            break;
         case 'f':
            debugFile = TRUE;
            break;
         case 'i':
            inifile = optarg;
            break;
         case 's':
            debugSocket = TRUE;
            break;
         default:
            fprintf(stderr, "usage: ackserver -?adfs\n\n");
            exit(1);
      }
   }

   if (debugFile) {
      GetPrivateProfileString("rsvserver", "debugfile", debugfilename,
         sizeof(debugfilename), "debug.rsv", inifile);

      if ((db = creat(debugfilename, DBGPERM)) < 0) {
         perror(progName);
         debugFile = FALSE;
      }
   }

   GetPrivateProfileString("rsvserver", "service", service, sizeof(service),
      "WDOCRSV", inifile);
   GetPrivateProfileString("rsvserver", "protocol", protocol,
      sizeof(protocol), "tcp", inifile);

   if (initaddress(&myaddr_in, service, protocol) < 0) {
      wescolog(NETWORKLOG,
         "%s: %s not found in /etc/services.  Server terminated!",
         progName, service);
      err_sys("error on service lookup");
   }

   if ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0)
      err_sys("error creating listen socket");

   daemoninit();
   AcceptConnections();

   return 1;
}

/*
 ****************************************************************************
 *
 *  Function:    AcceptConnections
 *
 *  Description: This function sits in an infinite loop and listens to
 *               the specified port for a conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
void AcceptConnections(void)
{
   size_t addrlen;
   int    count; 
   int    attempt;
   bool_t   servercalled;
   char   version[VERSIONLENGTH];
   char   *hostname;
   int    status;
   struct sigaction chldact;
   int    serrno;
   struct sigaction hupact;
	int lock_fd;

#if defined(_GNU_SOURCE) || defined(SVR4)
   /* ignore the SIGCLD signal, this will prevent zombie processes */
   chldact.sa_handler = SIG_IGN;
   sigemptyset(&chldact.sa_mask);
   sigaddset(&chldact.sa_mask, SIGALRM);
   chldact.sa_flags = 0;
   chldact.sa_flags |= (SA_NOCLDSTOP|SA_NOCLDWAIT);
   sigaction(SIGCLD, &chldact, NULL);
#else
   /* child processes must be waited for to remove zombie processes */
   chldact.sa_handler = removezombie;
   sigemptyset(&chldact.sa_mask);
   sigaddset(&chldact.sa_mask, SIGALRM);
   chldact.sa_flags = 0;
   chldact.sa_flags |= SA_NOCLDSTOP; /* no SIGCHLD when process is stopped */
   sigaction(SIGCHLD, &chldact, NULL);
#endif

   /* ignore the terminal hangup signal */
   hupact.sa_handler = SIG_IGN;
   sigemptyset(&hupact.sa_mask);
   sigaddset(&hupact.sa_mask, SIGALRM);
   sigaddset(&hupact.sa_mask, SIGCHLD);
   hupact.sa_flags = 0;
   sigaction(SIGHUP, &hupact, NULL);

   /*
      Set up an infinite loop and accept incoming conversations.
      The function accept() is a blocking call and waits until a
      conversation is received before returning.
   */
   for ( ; ; ) {
      addrlen = sizeof(peeraddr_in);
      if ((s = accept(ls, (struct sockaddr *)&peeraddr_in, &addrlen)) < 0) {
         serrno = errno;
         if (errno == EINTR)
            continue;

         close(ls);
         wescolog(NETWORKLOG, "%s: unable to accept connection", progName);
         while ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0) {
            serrno = errno;
            wescolog(NETWORKLOG, "%s: %s", progName, strerror(serrno));

            if (debugFile)
               wescolog(db, "%d: %s\n", getpid(), strerror(serrno));

            sleep(1);    /* give it a rest */
         }
         continue;
      }

		if ((lock_fd = open(WESCOM_LOCK_FILE, O_RDONLY)) >= 0) {
			/* do not allow a connection if branch is locked */
			if (debugFile)
				wescolog(db, "%d: quitting because of lock.WESCOM\n", getpid());

			close(lock_fd);
			close(s);
			continue;
		}

      /*
         Fork the process.  If the process is a child process then call
         the Server function.  If the process is the parent, close the
         client socket and resume the inifinite loop.  If the return value
         of fork is a -1, the fork process failed.  Alert someone.
      */
      switch (fork()) {
         case -1:        /* forking error */
            wescolog(NETWORKLOG, "%s: unable to fork daemon\n",
               progName);
            sleep(1);
            break;
         case 0:         /* child process */
            close(ls);    /* close the listen socket */
            attempt = 0;
            servercalled = FALSE;
            if (AuthorizeConnection(s, &hp, &peeraddr_in)) {
               while (!servercalled && attempt < 2) {
                  hostname = hp->h_name;
                  memset(version, 0, sizeof(version));
                  if (GetVersion(s, version, progName, hostname)) {
                     version[VERSIONLENGTH-1] = 0;
                     if (debugFile)
                        wescolog(db, "version = %s; attempt = %d\n",
                           version, attempt);

                     for (count = 0;
                        count < NUMVERSIONSUPPORTED && !servercalled;
                        count++) {
                           if (!strcmp(versionTable[count].number, version)) {
                              if (debugFile)
                                 wescolog(db, "calling server\n");

                              SendBuffer(progName, s, "OK       ",
                                 VERSIONLENGTH);
                              versionTable[count].Server(hostname,
                                 versionTable[count].cobolProgram,
                                 1024);
                              servercalled = TRUE;

                              if (debugFile)
                                 wescolog(db, "returned from server\n");
                           }
                     }
                  }

                  if (!servercalled)
                      SendBuffer(progName, s,
                          versionTable[NUMVERSIONSUPPORTED-1].number,
                          VERSIONLENGTH);

                  attempt++;
               }
            }
            else {
                hostname = inet_ntoa(peeraddr_in.sin_addr);
                wescolog(NETWORKLOG,
                    "%s: host at %s unknown.  Security disconnect!",
                    progName, hostname);
                close(s);
            }
            exit(0);
            break;
         default:        /* parent process */
            close(s);
            while(waitpid(-1, &status, WNOHANG) > 0);
      }
   }

}

/*
 ****************************************************************************
 *
 *  Function:    Server
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server060400(char *hostName, char *cobolFunc, int bufferSize)
{
   int fd;
   char partNum[4];
   char dirName[256];
   char path[FILENAMELENGTH];
   char cmd[FILENAMELENGTH];
   char datafile[FILENAMELENGTH];
   char buffer[ERRORDESCLENGTH];
   int bytesReceived = 0;
   OLDFUNCPARMS parameters;
   FUNCPARMS newparms;
	char * argv[3] = {
		"/usr/lbin/runcbl",
		"REMRSV",
		NULL
	};

   if (debugFile)
      wescolog(db, "%s: Connected!\n", hostName);

   /* Initialize the parameters buffer to all spaces */
   memset(&parameters, 32, sizeof(parameters));

   /*
      Read in the message sent from the client.  If no data is received
      display an error message and exit the function.
   */
   bytesReceived = ReceiveData(s, (char*)&parameters, sizeof(parameters), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG, 
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      return;
   }

   memcpy(&newparms, &parameters, 30);
   memcpy(newparms.status, parameters.status, sizeof(parameters) - 30);

   memset(partNum, 0, sizeof(partNum));
   if (!GetPartition(partNum, cobolFunc, dirName)) {
      strncpy(parameters.status, "11", 2);
      SendBuffer(progName, s, (char*)&parameters, sizeof(parameters));
      return;
   }

   strncpy(newparms.partition, partNum, 3);

   /*
      The COBOL routine will be called here to process the requested
      SIM #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   GetPrivateProfileString("rsvserver", "cobol_path", path, sizeof(path),
      "/usr/lbin/runcbl", inifile);
   GetPrivateProfileString("rsvserver", "datafile", datafile, sizeof(datafile),
      "t_linkage", inifile);

/*
   sprintf(cmd, "%s %s", path, cobolFunc);
   if (call_cobol(cmd, datafile, &parameters, sizeof(parameters)) < 0) {
*/
	if (call_cobol(s, path, argv, datafile, &parameters, sizeof(parameters)) < 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parameters.status, "05", 2);
      CtoCobol(parameters.retname, "system()", sizeof(parameters.retname));
      CtoCobol(parameters.retdesc, buffer, sizeof(parameters.retdesc));
      SendBuffer(progName, s, (char*)&parameters, sizeof(parameters));
      DeletePartition(dirName);
      return;
   }

   memcpy(parameters.status, newparms.status, sizeof(parameters) - 30);

   if (debugFile)
      wescolog(db, "%s: Exited Cobol Function\n", hostName);

   /* Send the result code of the reservation function call */
   if (debugFile)
      wescolog(db, "%s: result -> %c%c\n", hostName, parameters.status,
         parameters.status);

   if (SendBuffer(progName, s, (char*)&parameters, sizeof(parameters))) {
      if (debugFile)
         wescolog(db, "%s: Conversation terminated normally.\n", hostName);
   }
	
   shutdown(s, 2);
   close(s);

   DeletePartition(dirName);
   exit(0);
}

/*
 ****************************************************************************
 *
 *  Function:    Server060401
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server060500(char *hostName, char *cobolFunc, int bufferSize)
{
   char partNum[4];
   char dirName[256];
   char path[FILENAMELENGTH];
   char cmd[FILENAMELENGTH];
   char datafile[FILENAMELENGTH];
   char buffer[ERRORDESCLENGTH];
   int bytesReceived = 0;
   FUNCPARMS parameters;
	char * argv[3] = {
		"/usr/lbin/runcbl",
		"REMRSV",
		NULL
	};
   char dt[513];
   int i;

   argv[1] = cobolFunc;

   if (debugFile)
      wescolog(db, "%s: Connected!\n", hostName);

   /* Initialize the parameters buffer to all spaces */
   memset(&parameters, 32, sizeof(parameters));

   /*
      Read in the message sent from the client.  If no data is received
      display an error message and exit the function.
   */
   bytesReceived = ReceiveData(s, (char*)&parameters, sizeof(parameters), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG, 
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      return;
   }

   memset(parameters.hostname, 32, HOSTNAMELENGTH);
   strncpy(parameters.hostname, hostName, strlen(hostName));

   memset(partNum, 0, sizeof(partNum));
   if (!GetPartition(partNum, cobolFunc, dirName)) {
      strncpy(parameters.status, "11", 2);
      SendBuffer(progName, s, (char*)&parameters, sizeof(parameters));
      return;
   }

   strncpy(parameters.partition, partNum, 3);

   /*
      The COBOL routine will be called here to process the requested
      SIM #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   GetPrivateProfileString("rsvserver", "cobol_path", path, sizeof(path),
      "/usr/lbin/runcbl", inifile);
   GetPrivateProfileString("rsvserver", "datafile", datafile, sizeof(datafile),
      "t_linkage", inifile);

   if (debugFile) {
      memcpy(dt, parameters.details, sizeof(parameters.details));
      dt[512] = 0;
      for (i = 0; i < sizeof(dt); i++) {
         if (dt[i] == 0) {
            dt[i] = ' ';
         }
      }
      wescolog(db, "Dump:\n%s\n", dt);
   }

/*
   sprintf(cmd, "%s %s", path, cobolFunc);
   if (call_cobol(cmd, datafile, &parameters, sizeof(parameters)) < 0) {
*/
	if (call_cobol(s, path, argv, datafile, &parameters, sizeof(parameters)) < 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parameters.status, "05", 2);
      CtoCobol(parameters.retname, "system()", sizeof(parameters.retname));
      CtoCobol(parameters.retdesc, buffer, sizeof(parameters.retdesc));
      SendBuffer(progName, s, (char*)&parameters, sizeof(parameters));
      DeletePartition(dirName);
      return;
   }

   if (debugFile)
      wescolog(db, "\n%s: Exited Cobol Function\n", hostName);

   /* Send the result code of the reservation function call */
   if (debugFile)
      wescolog(db, "%s: result -> %c%c\n", hostName, parameters.status[0],
         parameters.status[1]);

   if (SendBuffer(progName, s, (char*)&parameters, sizeof(parameters))) {
      if (debugFile)
         wescolog(db, "%s: Conversation terminated normally.\n", hostName);
   }

   shutdown(s, 2);
   close(s);

   DeletePartition(dirName);
   exit(0);
}
