#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#if defined(_hpux)
#   define _INCLUDE_POSIX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _INCLUDE_HPUX_SOURCE
#endif

#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#include <termios.h>
#endif


#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/ttold.h>
#endif

#include <string.h>

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "more.h"

/* ARGSUSED */
void GenClient060500(int s, GENTRANSAREA *parameters, int size)
{
   REMOTEFILE tempFile;
   int bytesReceived;
   int opMode;
   char opModeStr[OPMODELENGTH + 1];

   /* Send the data to the remote computer. */
   alarm(30);
   if (!SendBuffer("WDOCGENERIC", s, (char *)parameters, sizeof(GENTRANSAREA))) {
      strncpy(parameters->status, "03", sizeof(parameters->status));
      strncpy(parameters->error.name, "sending", 7);
      strncpy(parameters->error.description, "GENTRANSAREA", 12);
      close(s);
      return;
   }
   alarm(0);

   CobolToC(opModeStr, parameters->opmode, sizeof(opModeStr), 
      sizeof(parameters->opmode));
   sscanf(opModeStr, "%02d", &opMode);

   switch (opMode) {
      case SPECIFIEDPART_TRANS:
      case CREATEDPART_TRANS:
         break;
      case SPECIFIEDPART_INFILE:
      case CREATEDPART_INFILE:
      case FILE_SEND:
         strncpy(tempFile.name, parameters->clientsendfile, 
            sizeof(tempFile.name));
         if (!FileSend(s, tempFile)) {
            strncpy(parameters->status, "16", sizeof(parameters->status));
            close(s);
            return;
         }
         break;
      case SPECIFIEDPART_OUTFILE:
      case CREATEDPART_OUTFILE:
      case FILE_RETRIEVE:
         if (!FileReceive(s, parameters->clientrecvfile)) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }
         break;
      case SPECIFIEDPART_IOFILES:
      case CREATEDPART_IOFILES:
      case FILE_SENDRETRIEVE:
         strncpy(tempFile.name, parameters->clientsendfile, 
            sizeof(tempFile.name));
         if (!FileSend(s, tempFile)) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         if (!FileReceive(s, parameters->clientrecvfile)) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }
         break;
      case SPECIFIEDPART_MULTIN:
      case CREATEDPART_MULTIN:
      case FILE_MULTISEND:
         strncpy(tempFile.name, parameters->clientsendfile, 
            sizeof(tempFile.name));
         if (!FileMultiSend(s, tempFile)) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }
         break;
      case SPECIFIEDPART_MULTOUT:
      case CREATEDPART_MULTOUT:
      case FILE_MULTIRETRIEVE:
         if (!FileMultiReceive(s, parameters->clientrecvfile)) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }
         break;
      case SPECIFIEDPART_MULTIO:
      case CREATEDPART_MULTIO:
      case FILE_MULTISENDRETRIEVE:
         strncpy(tempFile.name, parameters->clientsendfile, 
            FILENAMELENGTH);
         if (!FileMultiSend(s, tempFile)) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         if (!FileMultiReceive(s, parameters->clientrecvfile)) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }
         break;
      default:
         strncpy(parameters->status, "18", 2);
         strncpy(parameters->error.name, "invalid", 7);
         strncpy(parameters->error.description, opModeStr, 2);
         close(s);
         return;
   };

   bytesReceived = ReceiveData(s, (char *)parameters, sizeof(GENTRANSAREA), 0);
   if (bytesReceived < sizeof(GENTRANSAREA)) {
      strncpy(parameters->status, "03", 2);
      strncpy(parameters->error.name, "receiving", 9);
      strncpy(parameters->error.description, "GENTRANSAREA", 12);
   }

   close(s);
}
