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
#include "sub.h"
#include "more.h"
#include "maint.h"
#include "process.h"

/*
 ****************************************************************************
 *
 *  Function:    Server
 *
 *  Version:     06.05.00
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
void Server060500(char *hostName, char *cobolFunc, int bufferSize)
{
	char partNum[3], dirName[256], buffer[128];
	char sku[20];
	int totalBytes = 0, bytesReceived = 0, pos = 0;
	Argument cobolargs[1];
	FUNCPARMS parameters;
	ERRORSTR *error;
	time_t timeStamp;

	if (processTable[myindex].debugFile) {
		sprintf(buffer, "%s: Connected!\n", hostName);
		writetolog(db, buffer, strlen(buffer), FALSE);
	}

	/* Initialize the parameters buffer to all spaces. */
	memset(&parameters, 32, sizeof(COBOLPARMS));

	/* 
		Set the sockets to linger so that the sockets will not close until
		the client has received the entire packet.
	*/
	setsocketlinger(s, ON, 2);

	/*
		Read in the message sent from the client.  If no data is received
		display an error message and exit the function.
	*/
	bytesReceived = ReceiveData(s, (char*)&parameters, sizeof(FUNCPARMS), 0);
	if (bytesReceived < 1) {
		time(&timeStamp);
		sprintf(networkErrorMessage, 
			"%s: %s: receive error...connection terminated unexpedtedly",
			progName, ctime(&timeStamp));
		NetworkError(networkErrorMessage, FALSE);
		return;
	}

	/*
		The COBOL routine will be called here to process the requested
		SIM #.
	*/
	cobolargs[0].a_address = (char *)&parameters;
	cobolargs[0].a_length = sizeof(FUNCPARMS);

	if (processTable[myindex].debugFile) {
		time(&timeStamp);
		sprintf(buffer, "%s: Entering COBOL program %s\n",
			progName, cobolFunc);
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
			sprintf(buffer, "%s: COBOL Error code -> %d\n", 
				hostName, A_call_err);
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	}

	/* Send the result code of the maintainence function call. */
	
	if (processTable[myindex].debugFile) {
		sprintf(buffer, "%s: return code - %c%c\n",
			hostName, parameters.status[0],
			parameters.status[1]);
		writetolog(db, buffer, strlen(buffer), TRUE);
	}

	if (!SendBuffer(progName, s, (char*)&parameters, sizeof(FUNCPARMS))) {
		time(&timeStamp);
		sprintf(networkErrorMessage,
			"%s: %s: receive error...connection terminated unexpedtedly",
			progName, ctime(&timeStamp));
		NetworkError(networkErrorMessage, FALSE);

		if (processTable[myindex].debugFile) {
			sprintf(buffer, "%s: Conversation interrupted; errno = %d\n",
				hostName, errno);
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	}
	else
		if (processTable[myindex].debugFile) {
			sprintf(buffer, "%s: Conversation terminated normally\n",
				hostName);
			writetolog(db, buffer, strlen(buffer), TRUE);
		}
	
	acu_cancel(cobolFunc);
}
