#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "more.h"
#include "sub.h"
#include "maint.h"
#include "process.h"

/*
 ****************************************************************************
 *
 *  Function:    Server
 *
 *  Version:     06.04.00
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
void Server060400(char *hostName, char *cobolFunc, int bufferSize)
{
	char partNum[3], dirName[256];
	char sku[20];
	int totalBytes = 0, bytesReceived = 0, pos = 0;
	Argument cobolargs[1];
	COBOLPARMS parameters;
	time_t timeStamp;
	char buffer[128];

	if (debugFile) {
		sprintf(buffer, "%s: Connected!\n", hostName);
		writetolog(db, buffer, strlen(buffer), TRUE);
	}

	/* Initialize the parameters buffer to all spaces. */
	memset(&parameters, 32, sizeof(COBOLPARMS));

	setsocketlinger(s, ON, 2);

	/*
		Read in the message sent from the client.  If no data is received
		display an error message and exit the function.
	*/
	bytesReceived = ReceiveData(s, (char*)&parameters, bufferSize, 0);
	if (bytesReceived < 1) {
		time(&timeStamp);
		sprintf(networkErrorMessage, 
			"%s: %s: receive error...connection terminated unexpectedly",
			progName, ctime(&timeStamp));
		NetworkError(networkErrorMessage, FALSE);
		return;
	}

	if (processTable[myindex].debugFile) {
		sprintf(buffer, "Received buffer from %s:\n", hostName);
		writetolog(db, buffer, strlen(buffer), TRUE);
		writetolog(db, (char *)&parameters, sizeof(parameters), FALSE);
	}

	/*
		The COBOL routine will be called here to process the requested
		SIM #.
	*/
	cobolargs[0].a_address = (char *)&parameters;
	cobolargs[0].a_length = sizeof(COBOLPARMS);

	if (processTable[myindex].debugFile) {
		time(&timeStamp);
		sprintf(buffer, "Entering COBOL program %s\n", cobolFunc);
		writetolog(db, buffer, strlen(buffer), TRUE);
	}

	if (cobol(cobolFunc, 1, cobolargs, 1)) {
		strncpy(parameters.status, "05", STATUSLENGTH);
		time(&timeStamp);
		sprintf(networkErrorMessage,
			"%s: %s: error running COBOL program %s, COBOL error code %d",
			progName, ctime(&timeStamp), cobolFunc, A_call_err);
		NetworkError(networkErrorMessage, TRUE);

		if (processTable[myindex].debugFile) {
			sprintf(buffer, "COBOL Error code -> %d\n", A_call_err);
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	}

	/* Send the result code of the maintainence function call. */
	
	if (processTable[myindex].debugFile) {
		time(&timeStamp);
		sprintf(buffer, "return code - %c%c\n",
			parameters.status[0], parameters.status[1]);
		writetolog(db, buffer, strlen(buffer), TRUE);
	}

	if (!SendBuffer(progName, s, (char*)&parameters, bufferSize)) {
		time(&timeStamp);
		sprintf(networkErrorMessage,
			"%s: %s: receive error...connection terminated unexpedtedly",
			progName, ctime(&timeStamp));
		NetworkError(networkErrorMessage, FALSE);
		
		if (processTable[myindex].debugFile) {
			sprintf(buffer, "Conversation interrupted; errno = %d\n", errno);
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	}
	else
		if (processTable[myindex].debugFile) {
			sprintf(buffer, "Conversation terminated normally\n");
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	
	acu_cancel(cobolFunc);
}
