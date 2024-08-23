/*
 ******************************************************************************
 * History:
 * 11/13/06 mjb 16067 - Modified SIGCHLD handling to properly clean up defunct
 *                      child processes.
 ******************************************************************************
*/
#include <sys/types.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <wait.h>
#include <stdlib.h>

#include "common.h"
#include "update.h"
#include "ackservr.h"
#include "part.h"

void removezombie(int);

char *inifile = WESCOMINI;

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
 *  Returns:     1 - it should never exit from this function
 *
 ****************************************************************************
*/
/* ARGSUSED */
int main(int argc, char *argv[], char *envp[])
{
   int option;
   extern char *optarg;

   /* set file creation mask */
   umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

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
      GetPrivateProfileString("ackserver", "debugfile", debugfilename,
         sizeof(debugfilename), "debug.ack", inifile);

      if ((db = creat(debugfilename, DBGPERM)) < 0) {
         perror(progName);
         debugFile = FALSE;
      }
   }

   GetPrivateProfileString("ackserver", "service", service, sizeof(service),
      "WDOCRSV", inifile);
   GetPrivateProfileString("ackserver", "protocol", protocol,
      sizeof(protocol), "tcp", inifile);

   if (debugFile)
      wescolog(db, "%d: using service %s, protocol %s\n", getpid(), service,
         protocol);

   if (initaddress(&myaddr_in, service, protocol) < 0) {
      wescolog(NETWORKLOG,
         "%s: %s not found in /etc/services.  Server terminated!",
         progName, service);
      err_sys("error on service lookup");
   }

   if (debugFile)
      wescolog(db, "%d: server address initialized\n", getpid());

   if ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0)
      err_sys("error creating listen socket");

   if (debugFile)
      wescolog(db, "%d: listen socket created successfully\n", getpid());

   daemoninit();

   if (debugFile)
      wescolog(db, "%d: genserver daemon is running\n", getpid());

   AcceptConnections();

   return 1;
}

/*
 ****************************************************************************
 *
 *  Function:    AcceptConnections
 *
 *  Description: This function sits in an infinite loop waiting for
 *               a client to initiate a conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
void AcceptConnections(void)
{            
   size_t addrLen;
	int attempt, count, serverCalled;
   char *hostName, version[VERSIONLENGTH];
   struct sigaction chldact;
   int serrno;
   char *msg;
   struct sigaction hupact;
	int lock_fd;

   chldact.sa_handler = removezombie;
   sigemptyset(&chldact.sa_mask);
   sigaddset(&chldact.sa_mask, SIGALRM);
   chldact.sa_flags = 0;
   chldact.sa_flags |= SA_NOCLDSTOP;
   sigaction(SIGCHLD, &chldact, NULL);

   hupact.sa_handler = SIG_IGN;
   sigemptyset(&hupact.sa_mask);
   sigaddset(&hupact.sa_mask, SIGALRM);
   sigaddset(&hupact.sa_mask, SIGCHLD);
   hupact.sa_flags = 0;
   sigaction(SIGHUP, &hupact, NULL);

   /*
      Set up an infinite loop and accept an incoming conversation.
      The function accept is a blocking call and waits until a
      Conversation is received before returning.
   */
   for (;;) {
      if (debugFile)
         wescolog(db, "%d: listening for connections on socket %d\n",
            getpid(), ls);

      addrLen = sizeof(struct sockaddr_in);
      if ((s = accept(ls, (struct sockaddr *)&peeraddr_in, &addrLen)) < 0) {
         if (errno == EINTR)
            continue;

         close(ls);
         wescolog(NETWORKLOG, "%s: unable to accept connection; errno = %d",
            progName, errno);
         while ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0) {
            serrno = errno;
            switch (ls) {
               case -1:
                  msg = "error creating socket";
                  break;
               case -2:
                  msg = "error binding socket";

                  /* we can try to reset the address for the server */
                  initaddress(&myaddr_in, service, protocol);
                  break;
               case -3:
                  msg = "error setting listen buffer";
                  break;
             }

             wescolog(NETWORKLOG, "%s: %s; errno = %d", progName,
                msg, serrno);

            if (debugFile)
               wescolog(db, "%d: %s; errno = %d\n", getpid(), msg, serrno);

            sleep(30);    /* give it a rest */
         }
         continue;
      }
      else {
			if ((lock_fd = open(WESCOM_LOCK_FILE, O_RDONLY)) >= 0) {
				close(lock_fd);
				close(s);
				continue;
			}

         /*
            Fork the process.  If the process is a child process then call
            the Server function.  If the process is the parent close the
            client socket and resume the infinite loop.  If the return value
            of fork is a -1 the fork process failed.  Exit the program.
         */
         switch(fork()) {
            case -1:        /* forking error */
               wescolog(NETWORKLOG, 
                  "%s: unable to fork daemon - connected", progName);
               close(s);
               continue;
            case 0:     /* child process */
               if (AuthorizeConnection(s, &hp, &peeraddr_in)) {
                  hostName = hp->h_name;
                  serverCalled = FALSE;
                  attempt = 0;
                  while (!serverCalled && attempt < 2) {
                     if (GetVersion(s, version, progName, hostName)) {
                        if (debugFile)
                           wescolog(db, "%d: rx'd version = %s\n", 
                              getpid(), version);

                        for (count = 0; count < NUMVERSIONSUPPORTED; count++)
                           if (!strcmp(ackversionTable[count].number,version)) {
                              if (debugFile)
                                 wescolog(db, "%d: transmitting OK\n",
                                    getpid());

                              SendBuffer(progName, s, "OK      ",VERSIONLENGTH);
                              if (debugFile) {
                                 wescolog(db, "%d: calling server\n",
                                    getpid());
                                 wescolog(db, "%d: index = %d\n",
                                    getpid(), count);
                              }

                              ackversionTable[count].Server(hostName,
                                 ackversionTable[count].cobolProgram, 1024);
                              serverCalled = TRUE;
                           }
                           else
                              if (debugFile)
                                 wescolog(db, "%d: try again\n",
                                    getpid());

                     }

                     if (!serverCalled)
                        SendBuffer(progName, s,
                           ackversionTable[NUMVERSIONSUPPORTED-1].number,
                           VERSIONLENGTH);
                     attempt++;
                  }
               }
               else {
                  hostName = inet_ntoa(peeraddr_in.sin_addr);
                  wescolog(NETWORKLOG,
                     "%s: host at address %s unknown.  Security disconnect!",
                     progName, hostName);
               }

               exit(0);
               break;
            default:    /* parent process */
               close(s);
         }
      }
   }
}

/*
 ****************************************************************************
 *
 *  Function:    Server060400
 *
 *  Description: This function processes an accepted conversation.
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
   char buffer[256], c_counter[COUNTERLENGTH+1];
   int bytesReceived = 0, numRecords = 0;
   int sendBytes, currRecord, useFile = FALSE, strLength = 0;
   int done = FALSE, cobol_argc;
   char dirName[FILENAMELENGTH], partNum[10];
   RPOACKPARMS parameters;
   FILE *inFile;
   OLDFUNCPARMS recvBuffer;
   CCACKR11DETAIL *details;
   char path[FILENAMELENGTH], cmd[FILENAMELENGTH], datafile[FILENAMELENGTH];

   if (debugFile)
      wescolog(db, "%d: Processing 06.04.00 request from %s\n", getpid(),
         hostName);

   close(ls);    /* close listen socket */

   details = (CCACKR11DETAIL *)recvBuffer.details;

   if (debugFile)
      wescolog(db, "%d: Initialized ACU-COBOL\n", getpid());

   /* initialize memory */
   memset(&parameters, 32, sizeof(parameters));
   memset(partNum, 0, sizeof(partNum));

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 0) {
      wescolog(NETWORKLOG, "%s: %s", progName, strerror(errno));
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "%d: Received buffer from client\n", getpid());

   strncpy(parameters.branch, details->branch, sizeof(parameters.branch));
   strncpy(parameters.sku, details->sku, sizeof(parameters.sku));
   strncpy(parameters.transaction, recvBuffer.transaction, 
      sizeof(parameters.transaction));
   strncpy(parameters.mode, details->mode, sizeof(parameters.mode));
   strncpy(parameters.hostname, hostName, (int)strlen(hostName));

   if (!GetPartition(partNum, cobolFunc, dirName)) {
      strncpy(recvBuffer.status, "11", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      close(s);
      return;
   }

   if (debugFile)
      wescolog(db, "%d: Running in partition number %s\n", getpid(), partNum);

   strncpy(parameters.partition, partNum, sizeof(parameters.partition));

   /*
      The COBOL routine will be called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "%d: Running program %s\n", getpid(), cobolFunc);

   GetPrivateProfileString("ackserver", "cobol_path", path, sizeof(path),
      "/usr/lbin/runcbl", inifile);
   GetPrivateProfileString("ackserver", "datafile", datafile, sizeof(datafile),
      "t_linkage", inifile);

   sprintf(cmd, "%s %s", path, cobolFunc);
   if (call_cobol(cmd, datafile, &parameters, sizeof(parameters)) < 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(recvBuffer.status, "05", 2);
      CtoCobol(recvBuffer.retname, "system()", sizeof(recvBuffer.retname));
      CtoCobol(recvBuffer.retdesc, buffer, sizeof(recvBuffer.retdesc));
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      DeletePartition(dirName);
      return;
   }

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "%d: status -> %c%c\n", getpid(), parameters.status[0], 
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, 
      ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);
   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG,
         "%s: error sending buffer", progName);
      close(s);
      DeletePartition(dirName);
      return;
   }

   /*
      If the status returned from the AcuCOBOL program is 00, transmit
      the number of records returned to the client.  If a file is used
      to hold the data read the file; otherwise, use the memory buffer.
   */
   if (!strncmp(parameters.status, "00", 2)) {
      strncpy(c_counter, parameters.counter, COUNTERLENGTH);
      c_counter[COUNTERLENGTH] = 0;
      sendBytes = PORECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);

      if (debugFile)
         wescolog(db, "%d: number of records returned -> %s\n", getpid(),
            c_counter);

      if (parameters.filename[0] > ' ') {
         if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
            wescolog(NETWORKLOG, "%s: error opening file %s",
               progName, parameters.filename);
            close(s);
            DeletePartition(dirName);
            return;
         }
         useFile = TRUE;
      }

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /* 
               Read a record from the specified file until the end of
               file is reached.
            */
            if (!feof(inFile)) {
               fgets(buffer, PORECORDLENGTH+1, inFile);
               strLength = (int)strlen(buffer);
               memset(buffer+strLength, 32, PORECORDLENGTH - strLength);
            }
            else
               done = TRUE;
         }
         else {
            /*
               Increment the current record counter and read the next
               record in the buffer.
            */
            if (currRecord < numRecords)
               strncpy(buffer, parameters.results[currRecord++], 
                  PORECORDLENGTH);
            else
               done = TRUE;
         }

         if (!done) {
            if (!SendBuffer(progName, s, buffer, sendBytes)) {
               wescolog(NETWORKLOG, "%s: %s", progName, strerror(errno));
               close(s);
               DeletePartition(dirName);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   DeletePartition(dirName);
   shutdown(s, 2);
   close(s);

   if (debugFile)
      wescolog(db, "%s: Conversation terminated normally.\n", hostName);
}

/*
 ****************************************************************************
 *
 *  Function:    Server
 *
 *  Description: This function processes an accepted conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server060503(char *hostName, char *cobolFunc, int bufferSize)
{
   char buffer[256], c_counter[COUNTERLENGTH+1];
   int bytesReceived = 0, numRecords = 0;
   int sendBytes, currRecord, useFile = FALSE, strLength = 0;
   int done = FALSE;
   char dirName[FILENAMELENGTH], partNum[4];
   RPOACKPARMS parameters;
   FILE *inFile;
   FUNCPARMS recvBuffer;
   CCACKR11DETAIL *details;
   char path[FILENAMELENGTH], cmd[FILENAMELENGTH], datafile[FILENAMELENGTH];
   char * argv[3] = {
      "/usr/lbin/runcbl",
      "",
      NULL
   };

   argv[1] = cobolFunc;

   if (debugFile)
      wescolog(db, "%d: Processing request from %s\n", getpid(), hostName);

   close(ls);

   details = (CCACKR11DETAIL *)recvBuffer.details;

   memset(&parameters, 32, sizeof(parameters));

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, 
      sizeof(FUNCPARMS), 0);
   if (bytesReceived < 0) {
      wescolog(NETWORKLOG, "%s: %s", progName, strerror(errno));
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "%d: Received buffer from client\n", getpid());

   strncpy(parameters.branch, details->branch, BRANCHLENGTH);
   strncpy(parameters.sku, details->sku, SKULENGTH);
   strncpy(parameters.transaction, recvBuffer.transaction, TRANSACTIONSIZE);
   strncpy(parameters.mode, details->mode, MODELENGTH);
   strncpy(parameters.partition, recvBuffer.partition, PARTITIONLENGTH);
   strncpy(parameters.hostname, hostName, (int)strlen(hostName));

   if (debugFile)
      wescolog(db, "%d: Initialized COBOL parameters\n", getpid());

   memset(partNum, 0, sizeof(partNum));
   if (!GetPartition(partNum, cobolFunc, dirName)) {
      strncpy(recvBuffer.status, "11", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(FUNCPARMS));
      close(s);
      return;
   }

   if (debugFile)
      wescolog(db, "%d: Running in partition number %s\n", getpid(), partNum);

   strncpy(parameters.partition, partNum, 3);

   /*
      The COBOL routine will be called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "%d: Running program %s\n", getpid(), cobolFunc);

   GetPrivateProfileString("ackserver", "cobol_path", path, sizeof(path),
      "/usr/lbin/runcbl", inifile);
   GetPrivateProfileString("ackserver", "datafile", datafile, sizeof(datafile),
      "t_linkage", inifile);

/*
   sprintf(cmd, "%s %s", path, cobolFunc);
   if (call_cobol(cmd, datafile, &parameters, sizeof(parameters)) < 0) {
*/
   if (call_cobol(s, path, argv, datafile, &parameters, sizeof(parameters)) < 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(recvBuffer.status, "05", 2);
      CtoCobol(recvBuffer.retname, "system()", sizeof(recvBuffer.retname));
      CtoCobol(recvBuffer.retdesc, buffer, sizeof(recvBuffer.retdesc));
      SendBuffer(progName, s, (char *)&recvBuffer, sizeof(recvBuffer));
      DeletePartition(dirName);
      return;
   }

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile) 
      wescolog(db, "%s: status -> %c%c\n", hostName, parameters.status[0], 
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, 
      ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);
   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(FUNCPARMS))) {
      wescolog(NETWORKLOG,
         "%s: error sending buffer", progName);
      close(s);
      DeletePartition(dirName);
      return;
   }

   /*
      If the status returned from the AcuCOBOL program is 00, transmit
      the number of records returned to the client.  If a file is used
      to hold the data read the file; otherwise, use the memory buffer.
   */
   if (!strncmp(parameters.status, "00", 2)) {
      strncpy(c_counter, parameters.counter, COUNTERLENGTH);
      c_counter[COUNTERLENGTH] = 0;
      sendBytes = PORECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);

      if (debugFile) 
         wescolog(db, "%s: number of records returned -> %s", hostName,
            c_counter);

      if (parameters.filename[0] > ' ') {
         if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
            wescolog(NETWORKLOG, "%s: error opening file %s", progName,
               parameters.filename);
            close(s);
            DeletePartition(dirName);
            return;
         }
         useFile = TRUE;
      }

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /* 
               Read a record from the specified file until the end of
               file is reached.
            */
            if (!feof(inFile)) {
               fgets(buffer, PORECORDLENGTH+1, inFile);
               strLength = (int)strlen(buffer);
               memset(buffer+strLength, 32, PORECORDLENGTH - strLength);
            }
            else
               done = TRUE;
         }
         else {
            /*
               Increment the current record counter and read the next
               record in the buffer.
            */
            if (currRecord < numRecords)
               strncpy(buffer, parameters.results[currRecord++], 
                  PORECORDLENGTH);
            else
               done = TRUE;
         }

         if (!done) {
            if (!SendBuffer(progName, s, buffer, sendBytes)) {
               wescolog(NETWORKLOG, 
                  "%s: receive error...connection terminated unexpedtedly",
                  progName);
               close(s);
               DeletePartition(dirName);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   DeletePartition(dirName);
   shutdown(s, 2);
   close(s);

   if (debugFile) 
      wescolog(db, "%s: Conversation terminated normally.\n", hostName);
}


void removezombie(int status)
{
   int statloc;

   while (waitpid(-1, &statloc, WNOHANG) > 0);
}
