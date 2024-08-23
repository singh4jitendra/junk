/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Updated record size for bonded inventory.
 * 11/23/10 mjb 10024 - Updated record size for excess quantity
 ******************************************************************************
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"
#include "receive.h"
#include "update.h"

/*
   def3208  mjb  09/15/09  Updated the WDOC_VERSION value to 10.06.00.
*/
void WDOCGETINV(FUNCPARMS*);
void TWDOCGETINV(FUNCPARMS*);

/* 06142 */
#ifdef RECORDLENGTH
#undef RECORDLENGTH
/* csc-gaps
#define RECORDLENGTH 69
#define WDOC_VERSION "10.03.00"
#define RECORDLENGTH 96
#define WDOC_VERSION "10.06.00"
*/
#define RECORDLENGTH 104
#define WDOC_VERSION "10.09.01"
#endif

/*
 ****************************************************************************
 *
 * Function:    WDOCGETINV
 *
 * Description: This fuction gets the inventory amount off of WESDOC.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void WDOCGETINV(FUNCPARMS *parameters)
{
   int s;
   int totalBytes = 0;
   int timeout;
   bool_t done = FALSE;
   struct hostent *hp;
   struct servent *sp;
   struct sockaddr_in peeraddr;
   char service[25];
   char protocol[25];
   FILE *destFile;
   char c_HostName[HOSTNAMELENGTH+1];
   char c_FileName[FILENAMELENGTH+1];
   char record[256];
   CDINVR11DETAIL *details;
   char version[VERSIONLENGTH];
   struct sigaction alrmact;

   /* set up SIGALRM handler */
   alrmact.sa_handler = TCPAlarm;
   sigemptyset(&alrmact.sa_mask);
   alrmact.sa_flags = 0;
   sigaction(SIGALRM, &alrmact, NULL);

   /* initialize memory */
   memset(c_HostName, 0, sizeof(c_HostName));
   memset(c_FileName, 0, sizeof(c_FileName));
   memset(record, 0, sizeof(record));
   memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));

   /* read INI settings */
   GetPrivateProfileString("invserver", "version", version, sizeof(version),
      WDOC_VERSION, WESCOMINI); /* 06142 */
   GetPrivateProfileString("invserver", "service", service, sizeof(service),
      "WDOCINV", WESCOMINI);
   GetPrivateProfileString("invserver", "protocol", protocol,
      sizeof(protocol), "tcp", WESCOMINI);
   timeout = GetPrivateProfileInt("invserver", "timeout", 30, WESCOMINI);

   details = (CDINVR11DETAIL*)parameters->details;

   CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
      HOSTNAMELENGTH);
   CobolToC(c_FileName, details->filename, FILENAMELENGTH+1,
      FILENAMELENGTH);

   /*Resolve host name to entry in /etc/hosts file */
   if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
      strncpy(parameters->status, "01", 2);
      return;
   }

   /* Resolve service to entry in /etc/services file */
   if ((sp = getservbyname(service, "tcp")) == NULL) {
      strncpy(parameters->status, "09", 2);
      return;
   }

   /* set up peer address */
   peeraddr.sin_family = AF_INET;
   peeraddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   peeraddr.sin_port = sp->s_port;

   /* Create the socket */
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      strncpy(parameters->status, "03", 2);
      return;
   }

   /* Connect to the remote machine */
   alarm(timeout);
   if (connect(s, (struct sockaddr *)&peeraddr, sizeof(struct sockaddr_in)) < 0) {
      strncpy(parameters->status, "02", 2);
      close(s);
      return;
   }
   alarm(0);

   /* Send the version */
   alarm(timeout);
   if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
      strncpy(parameters->status, "03", 2);
      close(s);
      return;
   }
   alarm(0);

   /* Send the data */
   alarm(timeout);
   if (send(s, (char*)parameters, sizeof(FUNCPARMS), 0) != sizeof(FUNCPARMS)) {
      strncpy(parameters->status, "03", 2);
      close(s);
      return;
   }
   alarm(0);

   /* Get the return code */
   totalBytes = ReceiveData(s, (char*)parameters, sizeof(FUNCPARMS), 0);
   if (totalBytes == -1) {
      strncpy(parameters->status, "03", 2);
      close(s);
      return;
   }

   /* If return code is 00 open file and log data */
   if (!strncmp(parameters->status, "00", 2)) {
      if ((destFile = fopen(c_FileName, "wt")) == NULL) {
         strncpy(parameters->status, "04", 2);
         close(s);
         return;
      }

      /* Receive data from remote computer */
      while (!done) {
         totalBytes = ReceiveData(s, record, RECORDLENGTH, 0);
         if (totalBytes == -1) {
            strncpy(parameters->status, "03", 2);
            fclose(destFile);
            close(s);
            return;
         }
         else if (totalBytes == 0)
            done = TRUE;
         else {
/*            fprintf(destFile, "%s\n", record); */
            fwrite(record, RECORDLENGTH, 1, destFile);
            fprintf(destFile, "\n");
            fflush(destFile);
         }
      }
      fclose(destFile);
   }

   close(s);
}

/*
 ****************************************************************************
 *
 * Function:    TWDOCGETINV
 *
 * Description: This fuction gets the inventory amount off of WESDOC.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void TWDOCGETINV(FUNCPARMS *parameters)
{
   int s;
   int totalBytes = 0;
   int timeout;
   bool_t done = FALSE;
   struct hostent *hp;
   struct servent *sp;
   struct sockaddr_in peeraddr;
   char service[25];
   char protocol[25];
   FILE *destFile;
   FILE *logfile;
   char c_HostName[HOSTNAMELENGTH+1];
   char c_FileName[FILENAMELENGTH+1];
   char record[256];
   CDINVR11DETAIL *details;
   char version[VERSIONLENGTH];
   struct sigaction alrmact;

   /* set up SIGALRM handler */
   alrmact.sa_handler = TCPAlarm;
   sigemptyset(&alrmact.sa_mask);
   alrmact.sa_flags = 0;
   sigaction(SIGALRM, &alrmact, NULL);

   /* initialize memory */
   memset(c_HostName, 0, sizeof(c_HostName));
   memset(c_FileName, 0, sizeof(c_FileName));
   memset(record, 0, sizeof(record));
   memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));

   if ((logfile = fopen("TWDOCGETINV.log", "at")) != NULL)
   {
      fprintf(logfile,"Log\n");
   }
   /* read INI settings */
   GetPrivateProfileString("invserver", "version", version, sizeof(version),
      WDOC_VERSION, WESCOMINI); /* 06142 */
   GetPrivateProfileString("invserver", "service", service, sizeof(service),
      "WDOCINV", WESCOMINI);
   GetPrivateProfileString("invserver", "protocol", protocol,
      sizeof(protocol), "tcp", WESCOMINI);
   timeout = GetPrivateProfileInt("invserver", "timeout", 30, WESCOMINI);
   if (logfile != NULL)
   {
      fprintf(logfile,"Version  = %s\n", version);
      fprintf(logfile,"Service  = %s\n", service);
      fprintf(logfile,"Protocol = %s\n", protocol);
      fprintf(logfile,"timeout  = %d\n", timeout);
   }

   details = (CDINVR11DETAIL*)parameters->details;

   CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
      HOSTNAMELENGTH);
   CobolToC(c_FileName, details->filename, FILENAMELENGTH+1,
      FILENAMELENGTH);

   if (logfile != NULL)
   {
      fprintf(logfile,"Host = %s\n", c_HostName);
      fprintf(logfile,"File = %s\n", c_FileName);
   }
   /*Resolve host name to entry in /etc/hosts file */
   if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
      strncpy(parameters->status, "01", 2);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 01 - gethostbyname failed\n");
          fclose(logfile);
      }
      return;
   }

   /* Resolve service to entry in /etc/services file */
   if ((sp = getservbyname(service, "tcp")) == NULL) {
      strncpy(parameters->status, "09", 2);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 09 - getservbyname failed\n");
          fclose(logfile);
      }
      return;
   }

   /* set up peer address */
   peeraddr.sin_family = AF_INET;
   peeraddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   peeraddr.sin_port = sp->s_port;

   /* Create the socket */
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      strncpy(parameters->status, "03", 2);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 03 - socket failed(%d)\n",s);
          fclose(logfile);
      }
      return;
   }

   /* Connect to the remote machine */
   alarm(timeout);
   if (connect(s, (struct sockaddr *)&peeraddr, sizeof(struct sockaddr_in)) < 0) {
      strncpy(parameters->status, "02", 2);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 02 - connect failed\n");
          fclose(logfile);
      }
      close(s);
      return;
   }
   alarm(0);

   /* Send the version */
   alarm(timeout);
   if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
      strncpy(parameters->status, "03", 2);
      close(s);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 03 - send version failed\n");
          fclose(logfile);
      }
      return;
   }
   alarm(0);

   /* Send the data */
   alarm(timeout);
   if (send(s, (char*)parameters, sizeof(FUNCPARMS), 0) != sizeof(FUNCPARMS)) {
      strncpy(parameters->status, "03", 2);
      close(s);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 03 - send data failed\n");
          fclose(logfile);
      }
      return;
   }
   alarm(0);

   /* Get the return code */
   totalBytes = ReceiveData(s, (char*)parameters, sizeof(FUNCPARMS), 0);
   if (totalBytes == -1) {
      strncpy(parameters->status, "03", 2);
      close(s);
      if (logfile != NULL)
      {
          fprintf(logfile,"Status 03 - receive return code failed(%d)\n", totalBytes);
          fclose(logfile);
      }
      return;
   }

   /* If return code is 00 open file and log data */
   if (!strncmp(parameters->status, "00", 2)) {
      if ((destFile = fopen(c_FileName, "wt")) == NULL) {
         strncpy(parameters->status, "04", 2);
         close(s);
         if (logfile != NULL)
         {
             fprintf(logfile,"Status 04 - open failure on %s\n", c_FileName);
             fclose(logfile);
         }
         return;
      }

      /* Receive data from remote computer */
      while (!done) {
         totalBytes = ReceiveData(s, record, RECORDLENGTH, 0);
         if (totalBytes == -1) {
            strncpy(parameters->status, "03", 2);
            fclose(destFile);
            close(s);
            if (logfile != NULL)
            {
                fprintf(logfile,"Status 03 - receive data failed(%d)\n", totalBytes);
                fclose(logfile);
            }
            return;
         }
         else if (totalBytes == 0)
            done = TRUE;
         else {
/*            fprintf(destFile, "%s\n", record); */
            fwrite(record, RECORDLENGTH, 1, destFile);
            fprintf(destFile, "\n");
            fflush(destFile);
         }
      }
      fclose(destFile);
   }
   else
   {
      if (logfile != NULL)
      {
          fprintf(logfile,"returned status was %c%c\n",
                          parameters->status[0],
                          parameters->status[1]);
      }
   }
   if (logfile != NULL)
       fclose(logfile);

   close(s);
}
