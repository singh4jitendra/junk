#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef _GNU_SOURCE
#include <signal.h>
#else
#include <siginfo.h>
#endif

#include <wait.h>
#include <errno.h>

#include "common.h"
#include "receive.h"
#include "update.h"
#include "invservr.h"
#include "sub.h"

void logshutdown(void);
int setsignal(int);
void logsignal(int, struct siginfo *);
void sigchild_handler(int);

int isparent = TRUE;
char *wescomini = WESCOMINI;
char * cblconfig = NULL;

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
int main(int argc, char *argv[])
{
   int option;
   extern char *optarg;

   progName = argv[0];

   while ((option = getopt(argc, argv, "ac:fi:s")) != EOF) {
      switch (option) {
         case 'a':
            debugFile = debugSocket = TRUE;
            break;
         case 'c':
            cblconfig = optarg;
            break;
         case 'f':
            debugFile = TRUE;
            break;
         case 'i':
            wescomini = optarg;
            break;
         case 's':
               debugSocket = TRUE;
            break;
         default:
            fprintf(stderr, "usage: ackserver [ -afs -i <inifile> ]\n\n");
            exit(1);
      }
   }

   if (debugFile) {
      GetPrivateProfileString("invserver", "debugfile", debugfilename,
         sizeof(debugfilename), "debug.inv", wescomini);
      if ((db = creat(debugfilename, DBGPERM)) == NULL) {
         perror(progName);
         debugFile = FALSE;
      }
   }

   GetPrivateProfileString("invserver", "service", service, sizeof(service),
      "WDOCINV", wescomini);
   GetPrivateProfileString("invserver", "protocol", protocol,
      sizeof(protocol), "tcp", wescomini);

   if (initaddress(&myaddr_in, service, protocol) < 0) {
      wescolog(NETWORKLOG,
         "%s: %s not found in /etc/services.  Server terminated!",
         progName, service);
      err_sys("error on service lookup");
   }

   if ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0)
      err_sys("error creating listen socket");

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
   setsignal(SIGUSR1);
   setsignal(SIGUSR2);
   setsignal(SIGVTALRM);
   setsignal(SIGXCPU);
   setsignal(SIGXFSZ);

   atexit(logshutdown);

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
void AcceptConnections()
{
   int addrLen, count;
   char version[VERSIONLENGTH], *hostName;
   struct sigaction chldact;
   int socket_error;
   size_t so_size;
   short one = 1;
   int serrno;

#if 0
   chldact.sa_handler = SIG_IGN;
   sigemptyset(&chldact.sa_mask);
   sigaddset(&chldact.sa_mask, SIGALRM);
   chldact.sa_flags = 0;
   chldact.sa_flags |= (SA_NOCLDSTOP|SA_NOCLDWAIT);
   sigaction(SIGCLD, &chldact, NULL);
#else
   chldact.sa_handler = sigchild_handler;
   sigemptyset(&chldact.sa_mask);
   sigaddset(&chldact.sa_mask, SIGALRM);
   chldact.sa_flags = 0;
   chldact.sa_flags |= SA_NOCLDSTOP;
   sigaction(SIGCHLD, &chldact, NULL);
#endif

   /*
      Set up an infinite loop and accept an incoming conversation.
      The function accept is a blocking call and waits until a
      conversation is received before returning.
   */
   for (;;) {
      addrLen = sizeof(struct sockaddr_in);
      s = accept(ls, (struct sockaddr *)&peeraddr_in, &addrLen);
      if (s < 0) {
         serrno = errno;
         if (debugFile) {
            so_size = sizeof(socket_error);
            getsockopt(ls, SOL_SOCKET, SO_ERROR, (char *)&socket_error,
               (int *)&so_size);
            wescolog(db, "%s:ERROR:accept:%s\n", progName,
               strerror(errno));
            wescolog(db, "%s:INFO:socket errno:%d:%s\n", progName,
               socket_error, strerror(socket_error));
         }

         wescolog(NETWORKLOG, "%s:ERROR:accept:%s", progName, strerror(serrno));
         if ((serrno == EINTR) || (serrno == EPROTO) || (serrno == ECONNABORTED))
            continue;

         close(ls);
         sleep(1);
         while ((ls = getlistensocket(&myaddr_in, MAXRECEIVES)) < 0) {
            serrno = errno;
            wescolog(NETWORKLOG, "%s:ERROR:%s:%s", progName,
               ((ls == -1) ? "socket" : (ls == -2) ? "bind" : "listen"),
            strerror(serrno));
            if (debugFile) {
               so_size = sizeof(socket_error);
               getsockopt(ls, SOL_SOCKET, SO_ERROR, (char *)&socket_error,
                  (int *)&so_size);
               wescolog(db, "%s:ERROR:accept:%s\n", progName,
                  strerror(errno));
               wescolog(db, "%s:INFO:socket errno:%d:%s\n", progName,
                  socket_error, strerror(socket_error));
            }
            sleep(1);
         }

         if (debugSocket) setsocketdebug(ls, 1);
         setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
      }
      else {
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
               break;
            case 0:     /* child process */
               isparent = FALSE;
               if (AuthorizeConnection(s, &hp, &peeraddr_in)) {
                  hostName = hp->h_name;
                  if (GetVersion(s, version, progName, hostName)) {
                     if (debugFile)
                        wescolog(db, "%s: %s version %s\n", progName,
                           hostName, version);

                     setsocketlinger(s, 1, 1);

                     for (count = 0; count < NUMVERSIONSUPPORTED; count++)
                        if (!strcmp(versionTable[count].number, version))
                           versionTable[count].Server(hostName,
                              versionTable[count].cobolProgram,
                              versionTable[count].bufferSize);
                  }
               }
               else {
                  hostName = inet_ntoa(peeraddr_in.sin_addr);
                  wescolog(NETWORKLOG,
                     "%s: host at address %s unknown.  Security disconnect!",
                     progName, hostName);
                  close(s);
                  close(ls);
               }

               if (debugFile)
                  wescolog(db, "%d: exiting program\n", getpid());

               exit(0);
               break;
            default:    /* parent process */
               close(s);
               if (debugFile)
                  wescolog(db, "%d: closing client socket in parent process\n",
                     getpid());
         }
      }
   }
}

/*
 ****************************************************************************
 *
 *  Function:    Server060400
 *
 *  Description: This function processes the conversation.
 *
 *  Version:     06.04.00
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
   int done = FALSE, cobol_argc, count;
   char *cobol_argv[3];
   COBOLPARMS parameters;
   FILE *inFile;
   Argument args[1];
   OLDFUNCPARMS recvBuffer;
   CDINVR11DETAIL *details;

   details = (CDINVR11DETAIL *)recvBuffer.details;

   close(ls);

   memset(&parameters, 32, sizeof(parameters));

   if (debugFile)
      wescolog(db, "%d: %s connected\n", getpid(), hostName);

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG,
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "%d: received buffer\n", getpid());

   strncpy(parameters.branch, details->branch, sizeof(parameters.branch));
   strncpy(parameters.sku, details->sku, sizeof(parameters.sku));
   strncpy(parameters.transaction, recvBuffer.transaction,
      sizeof(parameters.transaction));

   if (debugFile)
      wescolog(db, "%d: branch #%c%c%c%c\n", getpid(), parameters.branch[0],
         parameters.branch[1], parameters.branch[2], parameters.branch[3]);

   /*
      The COBOL routine is called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "%d: running COBOL program %s\n", getpid(), cobolFunc);

   /* Initialize the COBOL runtime module */
   cobol_argv[0] = progName;
   cobol_argv[1] = "-b";
   cobol_argv[2] = cobolFunc;
   cobol_argc = 3;
   acu_initv(cobol_argc, cobol_argv);

   args[0].a_address = (char *)&parameters;
   args[0].a_length = sizeof(parameters);
   if (cobol(cobolFunc, 1, args)) {
      if (debugFile)
         wescolog(db, "%d: COBOL error code = %d\n", getpid(), A_call_err);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s", progName,
         cobolFunc);
      strncpy(recvBuffer.status, "05", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      acu_shutdown(1);
      return;
   }

   acu_shutdown(1);

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "%d: status = %c%c\n", getpid(), parameters.status[0],
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, sizeof(parameters.status));
   strncpy(recvBuffer.retname, parameters.error.name,
      sizeof(parameters.error.name));
   strncpy(recvBuffer.retdesc, parameters.error.description,
      sizeof(parameters.error.description));

   if ((parameters.filename[0] > ' ') &&
       (!strncmp(parameters.status, "00", 2))) {
      for (count = 0; count < FILENAMELENGTH; count++)
         if (parameters.filename[count] == ' ') {
            parameters.filename[count] = 0;
            break;
         }

      if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
         wescolog(NETWORKLOG, "%s: Can not open file %s", progName,
            parameters.filename);
         if (debugFile)
            wescolog(db, "%d: can not open file %s", getpid(),
               parameters.filename);
         strncpy(recvBuffer.status, "13", STATUSLENGTH);
      }
      else
         useFile = TRUE;
   }

   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG, "%s: error sending buffer", progName);
      close(s);
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
      sendBytes = RECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);
      if (debugFile)
         wescolog(db, "number of records -> %s\n", c_counter);

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /*
               Read a record from the specified file until the end of
               file is reached.
            */
            if ((!feof(inFile)) && (currRecord < numRecords)) {
               memset(buffer, 0, sizeof(buffer));
               fscanf(inFile, "%s\n", buffer);
               strLength = strlen(buffer);
               memset(buffer+strLength, 32, RECORDLENGTH - strLength);
               currRecord++;
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
               strncpy(buffer, parameters.results[currRecord],
                  RECORDLENGTH);
            else
               done = TRUE;

            currRecord++;
         }

         if (!done) {
            if (debugFile)
               wescolog(db, "record #%d -> %s\n", currRecord-1, buffer);

            if (!SendBuffer(progName, s, buffer, RECORDLENGTH)) {
               wescolog(NETWORKLOG, "%s: error sending buffer", progName);
               close(s);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   if (debugFile)
      wescolog(db, "%d: shutting down\n", getpid());

   shutdown(s, 2);
   close(s);
}

/*
 ****************************************************************************
 *
 *  Function:    Server060500
 *
 *  Description: This function processes the conversation.
 *
 *  Version:     06.05.00
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
   int done = FALSE, cobol_argc, count;
   char *cobol_argv[5];
   COBOLPARMS parameters;
   FILE *inFile;
   Argument args[1];
   FUNCPARMS recvBuffer;
   CDINVR11DETAIL *details;

   details = (CDINVR11DETAIL *)recvBuffer.details;

   close(ls);

   memset(&parameters, 32, sizeof(parameters));

   if (debugFile)
      wescolog(db, "Connected -> %s\n", hostName);

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG,
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "Received buffer!\n", (char *)&recvBuffer);

   strncpy(parameters.branch, details->branch, BRANCHLENGTH);
   strncpy(parameters.sku, details->sku, SKULENGTH);
   strncpy(parameters.transaction, recvBuffer.transaction, TRANSACTIONSIZE);

   if (debugFile)
      wescolog(db, "branch -> %c%c%c%c\n", parameters.branch[0],
         parameters.branch[1], parameters.branch[2], parameters.branch[3]);

   /*
      The COBOL routine is called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   /* Initialize the COBOL runtime module */
   cobol_argv[0] = progName;
   cobol_argv[1] = "-b";
   cobol_argv[2] = cobolFunc;

   if (cblconfig != NULL) {
      cobol_argv[3] = "-c";
      cobol_argv[4] = cblconfig;
      cobol_argc = 5;
   }
   else {
      cobol_argc = 3;
   }

   acu_initv(cobol_argc, cobol_argv);

   args[0].a_address = (char *)&parameters;
   args[0].a_length = sizeof(parameters);
   if (cobol(cobolFunc, 1, args)) {
      if (debugFile)
         wescolog(db, "COBOL error code -> %d\n", A_call_err);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s", progName,
         cobolFunc);
      strncpy(recvBuffer.status, "05", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      acu_shutdown(1);
      return;
   }

   acu_shutdown(1);

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "status -> %c%c\n", parameters.status[0],
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);

   if ((parameters.filename[0] > ' ') && (!strncmp(parameters.status, "00", 2))) {
      for (count = 0; count < FILENAMELENGTH; count++)
         if (parameters.filename[count] == ' ') {
            parameters.filename[count] = 0;
            break;
         }

      if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
         wescolog(NETWORKLOG, "%s: Can not open file %s", progName,
            parameters.filename);
         if (debugFile)
            wescolog(db, "filename -> %s", parameters.filename);
         strncpy(recvBuffer.status, "13", STATUSLENGTH);
      }
      else
         useFile = TRUE;
   }

   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG, "%s: error sending buffer", progName);
      close(s);
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
      sendBytes = RECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);
      if (debugFile)
         wescolog(db, "number of records -> %s\n", c_counter);

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /*
               Read a record from the specified file until the end of
               file is reached.
            */
            if ((!feof(inFile)) && (currRecord < numRecords)) {
               memset(buffer, 0, sizeof(buffer));
               fscanf(inFile, "%s\n", buffer);
               strLength = strlen(buffer);
               memset(buffer+strLength, 32, RECORDLENGTH - strLength);
               currRecord++;
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
               strncpy(buffer, parameters.results[currRecord],
                  RECORDLENGTH);
            else
               done = TRUE;

            currRecord++;
         }

         if (!done) {
            if (debugFile)
               wescolog(db, "record #%d -> %s\n", currRecord-1, buffer);

            if (!SendBuffer(progName, s, buffer, RECORDLENGTH)) {
               wescolog(NETWORKLOG, "%s: error sending buffer", progName);
               close(s);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   shutdown(s, 2);
   close(s);
}

/*
 ****************************************************************************
 *
 *  Function:    Server100300
 *
 *  Description: This function processes the conversation.
 *
 *  Version:     10.03.00
 *
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server100300(char *hostName, char *cobolFunc, int bufferSize)
{
   char buffer[256], c_counter[COUNTERLENGTH+1];
   int bytesReceived = 0, numRecords = 0;
   int sendBytes, currRecord, useFile = FALSE, strLength = 0;
   int done = FALSE, cobol_argc, count;
   char *cobol_argv[5];
   COBOLPARMS2 parameters;
   FILE *inFile;
   Argument args[1];
   FUNCPARMS recvBuffer;
   CDINVR11DETAIL *details;

   details = (CDINVR11DETAIL *)recvBuffer.details;

   close(ls);

   memset(&parameters, 32, sizeof(parameters));

   if (debugFile)
      wescolog(db, "Connected -> %s\n", hostName);

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG,
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "Received buffer!\n", (char *)&recvBuffer);

   strncpy(parameters.branch, details->branch, BRANCHLENGTH);
   strncpy(parameters.sku, details->sku, SKULENGTH);
   strncpy(parameters.transaction, recvBuffer.transaction, TRANSACTIONSIZE);
   strncpy(parameters.cust, details->cust, CUSTOMERLENGTH);

   if (debugFile)
      wescolog(db, "branch -> %c%c%c%c\n", parameters.branch[0],
         parameters.branch[1], parameters.branch[2], parameters.branch[3]);

   /*
      The COBOL routine is called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   /* Initialize the COBOL runtime module */
   cobol_argv[0] = progName;
   cobol_argv[1] = "-b";
   cobol_argv[2] = cobolFunc;

   if (cblconfig != NULL) {
      cobol_argv[3] = "-c";
      cobol_argv[4] = cblconfig;
      cobol_argc = 5;
   }
   else {
      cobol_argc = 3;
   }
   acu_initv(cobol_argc, cobol_argv);

   args[0].a_address = (char *)&parameters;
   args[0].a_length = sizeof(parameters);
   if (cobol(cobolFunc, 1, args)) {
      if (debugFile)
         wescolog(db, "COBOL error code -> %d\n", A_call_err);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s", progName,
         cobolFunc);
      strncpy(recvBuffer.status, "05", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      acu_shutdown(1);
      return;
   }

   acu_shutdown(1);

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "status -> %c%c\n", parameters.status[0],
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);

   if ((parameters.filename[0] > ' ') && (!strncmp(parameters.status, "00", 2))) {
      for (count = 0; count < FILENAMELENGTH; count++)
         if (parameters.filename[count] == ' ') {
            parameters.filename[count] = 0;
            break;
         }

      if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
         wescolog(NETWORKLOG, "%s: Can not open file %s", progName,
            parameters.filename);
         if (debugFile)
            wescolog(db, "filename -> %s", parameters.filename);
         strncpy(recvBuffer.status, "13", STATUSLENGTH);
      }
      else
         useFile = TRUE;
   }

   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG, "%s: error sending buffer", progName);
      close(s);
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
      sendBytes = V100300RECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);
      if (debugFile)
         wescolog(db, "number of records -> %s\n", c_counter);

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /*
               Read a record from the specified file until the end of
               file is reached.
            */
            if ((!feof(inFile)) && (currRecord < numRecords)) {
               memset(buffer, 32, sizeof(buffer));
/*
               fscanf(inFile, "%s\n", buffer);
               strLength = strlen(buffer);
               memset(buffer+strLength, 32, V100300RECORDLENGTH - strLength);
*/
               fread(buffer, V100300RECORDLENGTH, 1, inFile);

               currRecord++;
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
                 memcpy(buffer, parameters.results[currRecord], V100300RECORDLENGTH);
/*               strncpy(buffer, parameters.results[currRecord],
                  RECORDLENGTH); */
            else
               done = TRUE;

            currRecord++;
         }

         if (!done) {
            if (debugFile) {
               wescolog(db, "record #%d -> %s\n", currRecord-1, buffer);
            }

            if (!SendBuffer(progName, s, buffer, V100300RECORDLENGTH)) {
               wescolog(NETWORKLOG, "%s: error sending buffer", progName);
               close(s);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   shutdown(s, 2);
   close(s);
}
/*
 ****************************************************************************
 *
 *  Function:    Server100600
 *
 *  Description: This function processes the conversation.
 *
 *  Version:     10.06.00
 *
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server100600(char *hostName, char *cobolFunc, int bufferSize)
{
   char buffer[256], c_counter[COUNTERLENGTH+1];
   int bytesReceived = 0, numRecords = 0;
   int sendBytes, currRecord, useFile = FALSE, strLength = 0;
   int done = FALSE, cobol_argc, count;
   char *cobol_argv[5];

/* NOTE: change in structure name driven by record length change for
   record returned to branch.  There should also be a #define for the
   length of the record (in invservr.h), used to define the COBOLPARMS
   structure AND in code below to send the output back to the branch    */

   COBOLPARMS3 parameters;
   FILE *inFile;
   Argument args[1];
   FUNCPARMS recvBuffer;
   CDINVR11DETAIL *details;

   details = (CDINVR11DETAIL *)recvBuffer.details;

   close(ls);

   memset(&parameters, 32, sizeof(parameters));

   if (debugFile)
      wescolog(db, "Connected -> %s\n", hostName);

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG,
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "Received buffer!\n", (char *)&recvBuffer);

   strncpy(parameters.branch, details->branch, BRANCHLENGTH);
   strncpy(parameters.sku, details->sku, SKULENGTH);
   strncpy(parameters.transaction, recvBuffer.transaction, TRANSACTIONSIZE);
   strncpy(parameters.cust, details->cust, CUSTOMERLENGTH);

   if (debugFile)
      wescolog(db, "branch -> %c%c%c%c\n", parameters.branch[0],
         parameters.branch[1], parameters.branch[2], parameters.branch[3]);

   /*
      The COBOL routine is called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   /* Initialize the COBOL runtime module */
   cobol_argv[0] = progName;
   cobol_argv[1] = "-b";
   cobol_argv[2] = cobolFunc;

   if (cblconfig != NULL) {
      cobol_argv[3] = "-c";
      cobol_argv[4] = cblconfig;
      cobol_argc = 5;
   }
   else {
      cobol_argc = 3;
   }
   acu_initv(cobol_argc, cobol_argv);

   args[0].a_address = (char *)&parameters;
   args[0].a_length = sizeof(parameters);
   if (cobol(cobolFunc, 1, args)) {
      if (debugFile)
         wescolog(db, "COBOL error code -> %d\n", A_call_err);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s", progName,
         cobolFunc);
      strncpy(recvBuffer.status, "05", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      acu_shutdown(1);
      return;
   }

   acu_shutdown(1);

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "status -> %c%c\n", parameters.status[0],
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);

   if ((parameters.filename[0] > ' ') && (!strncmp(parameters.status, "00", 2))) {
      for (count = 0; count < FILENAMELENGTH; count++)
         if (parameters.filename[count] == ' ') {
            parameters.filename[count] = 0;
            break;
         }

      if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
         wescolog(NETWORKLOG, "%s: Can not open file %s", progName,
            parameters.filename);
         if (debugFile)
            wescolog(db, "filename -> %s", parameters.filename);
         strncpy(recvBuffer.status, "13", STATUSLENGTH);
      }
      else
         useFile = TRUE;
   }

   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG, "%s: error sending buffer", progName);
      close(s);
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
      sendBytes = V100600RECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);
      if (debugFile)
         wescolog(db, "number of records -> %s\n", c_counter);

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /*
               Read a record from the specified file until the end of
               file is reached.
            */
            if ((!feof(inFile)) && (currRecord < numRecords)) {
               memset(buffer, 32, sizeof(buffer));

               /* fscanf(inFile, "%s\n", buffer); */
               fgets(buffer, sizeof(buffer), inFile);
               if (buffer[strlen(buffer) - 1] == '\n') {
                  buffer[strlen(buffer) - 1] = 0;
               }

               strLength = strlen(buffer);
               memset(buffer+strLength, 32, V100600RECORDLENGTH - strLength);

               /* fread(buffer, V100600RECORDLENGTH, 1, inFile); */

               currRecord++;
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
                 memcpy(buffer, parameters.results[currRecord], V100600RECORDLENGTH);
/*               strncpy(buffer, parameters.results[currRecord],
                  RECORDLENGTH); */
            else
               done = TRUE;

            currRecord++;
         }

         if (!done) {
            if (debugFile) {
               wescolog(db, "record #%d -> %s\n", currRecord-1, buffer);
            }

            if (!SendBuffer(progName, s, buffer, V100600RECORDLENGTH)) {
               wescolog(NETWORKLOG, "%s: error sending buffer", progName);
               close(s);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   shutdown(s, 2);
   close(s);
}
/*
 ****************************************************************************
 *
 *  Function:    Server100901
 *
 *  Description: This function processes the conversation.
 *
 *  Version:     10.06.00
 *
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server100901(char *hostName, char *cobolFunc, int bufferSize)
{
   char buffer[256], c_counter[COUNTERLENGTH+1];
   int bytesReceived = 0, numRecords = 0;
   int sendBytes, currRecord, useFile = FALSE, strLength = 0;
   int done = FALSE, cobol_argc, count;
   char *cobol_argv[5];

/* NOTE: change in structure name driven by record length change for
   record returned to branch.  There should also be a #define for the
   length of the record (in invservr.h), used to define the COBOLPARMS
   structure AND in code below to send the output back to the branch    */

   COBOLPARMS4 parameters;
   FILE *inFile;
   Argument args[1];
   FUNCPARMS recvBuffer;
   CDINVR11DETAIL *details;

   details = (CDINVR11DETAIL *)recvBuffer.details;

   close(ls);

   memset(&parameters, 32, sizeof(parameters));

   if (debugFile)
      wescolog(db, "Connected -> %s\n", hostName);

   /*
      Get the data from the client.  Copy the data into the AcuCOBOL
      parameter list.
   */
   bytesReceived = ReceiveData(s, (char*)&recvBuffer, sizeof(recvBuffer), 0);
   if (bytesReceived < 1) {
      wescolog(NETWORKLOG,
         "%s: connection with %s terminated unexpedtedly",
         progName, hostName);
      close(s);
      exit(1);
   }

   if (debugFile)
      wescolog(db, "Received buffer!\n", (char *)&recvBuffer);

   strncpy(parameters.branch, details->branch, BRANCHLENGTH);
   strncpy(parameters.sku, details->sku, SKULENGTH);
   strncpy(parameters.transaction, recvBuffer.transaction, TRANSACTIONSIZE);
   strncpy(parameters.cust, details->cust, CUSTOMERLENGTH);

   if (debugFile)
      wescolog(db, "branch -> %c%c%c%c\n", parameters.branch[0],
         parameters.branch[1], parameters.branch[2], parameters.branch[3]);

   /*
      The COBOL routine is called here to process the requested
      SKU #.
   */
   if (debugFile)
      wescolog(db, "Running COBOL program %s\n", cobolFunc);

   /* Initialize the COBOL runtime module */
   cobol_argv[0] = progName;
   cobol_argv[1] = "-b";
   cobol_argv[2] = cobolFunc;

   if (cblconfig != NULL) {
      cobol_argv[3] = "-c";
      cobol_argv[4] = cblconfig;
      cobol_argc = 5;
   }
   else {
      cobol_argc = 3;
   }
   acu_initv(cobol_argc, cobol_argv);

   args[0].a_address = (char *)&parameters;
   args[0].a_length = sizeof(parameters);
   if (cobol(cobolFunc, 1, args)) {
      if (debugFile)
         wescolog(db, "COBOL error code -> %d\n", A_call_err);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s", progName,
         cobolFunc);
      strncpy(recvBuffer.status, "05", 2);
      SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer));
      acu_shutdown(1);
      return;
   }

   acu_shutdown(1);

   /*
      Send the data back to the client.  This will be a loop that sends
      one record at a time to the client.  This is dependent on the
      design of the data structure.  The status will be sent to the
      client first.
   */
   if (debugFile)
      wescolog(db, "status -> %c%c\n", parameters.status[0],
         parameters.status[1]);

   strncpy(recvBuffer.status, parameters.status, STATUSLENGTH);
   strncpy(recvBuffer.retname, parameters.error.name, ERRORNAMELENGTH);
   strncpy(recvBuffer.retdesc, parameters.error.description,
      ERRORDESCLENGTH);

   if ((parameters.filename[0] > ' ') && (!strncmp(parameters.status, "00", 2))) {
      for (count = 0; count < FILENAMELENGTH; count++)
         if (parameters.filename[count] == ' ') {
            parameters.filename[count] = 0;
            break;
         }

      if ((inFile = fopen(parameters.filename, "rt+")) == NULL) {
         wescolog(NETWORKLOG, "%s: Can not open file %s", progName,
            parameters.filename);
         if (debugFile)
            wescolog(db, "filename -> %s", parameters.filename);
         strncpy(recvBuffer.status, "13", STATUSLENGTH);
      }
      else
         useFile = TRUE;
   }

   if (!SendBuffer(progName, s, (char*)&recvBuffer, sizeof(recvBuffer))) {
      wescolog(NETWORKLOG, "%s: error sending buffer", progName);
      close(s);
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
      sendBytes = V100901RECORDLENGTH;
      sscanf(c_counter, "%4d", &numRecords);
      if (debugFile)
         wescolog(db, "number of records -> %s\n", c_counter);

      currRecord = 0;
      while (!done) {
         if (useFile) {
            /*
               Read a record from the specified file until the end of
               file is reached.
            */
            if ((!feof(inFile)) && (currRecord < numRecords)) {
               memset(buffer, 32, sizeof(buffer));

               /* fscanf(inFile, "%s\n", buffer); */
               fgets(buffer, sizeof(buffer), inFile);
               if (buffer[strlen(buffer) - 1] == '\n') {
                  buffer[strlen(buffer) - 1] = 0;
               }

               strLength = strlen(buffer);
               memset(buffer+strLength, 32, V100600RECORDLENGTH - strLength);

               /* fread(buffer, V100901RECORDLENGTH, 1, inFile); */

               currRecord++;
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
                 memcpy(buffer, parameters.results[currRecord], V100901RECORDLENGTH);
/*               strncpy(buffer, parameters.results[currRecord],
                  RECORDLENGTH); */
            else
               done = TRUE;

            currRecord++;
         }

         if (!done) {
            if (debugFile) {
               wescolog(db, "record #%d -> %s\n", currRecord-1, buffer);
            }

            if (!SendBuffer(progName, s, buffer, V100901RECORDLENGTH)) {
               wescolog(NETWORKLOG, "%s: error sending buffer", progName);
               close(s);
               return;
            }
         }
      }

      if (useFile)
         fclose(inFile);
   }

   shutdown(s, 2);
   close(s);
}
void logshutdown(void)
{
   if (isparent) {
      wescolog(NETWORKLOG, "%d: invserver was terminated", getpid());

      if (debugFile) {
         wescolog(db, "%d: processing normal shutdown\n", getpid());
         close(db);
      }

      shutdown(ls, 2);
      close(s);
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
   sigaddset(&sigact.sa_mask, SIGUSR1);
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

   if (debugFile) {
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
      case SIGQUIT:
         exit(1);
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

void sigchild_handler(int signo)
{
   pid_t pid;
   int status;

   while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
}
