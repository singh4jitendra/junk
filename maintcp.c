#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>

#include "common.h"
#include "receive.h"
#include "maint.h"

#define CLIENTVERSION "06.05.00"

void WDOCUPDCP(FUNCPARMS*);
void TWDOCUPDCP(FUNCPARMS*);

/*
 ****************************************************************************
 *
 * Function:    WDOCUPDINV
 *
 * Description: This fuction updates the inventory on WESDOC
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void WDOCUPDCP(FUNCPARMS *parameters)
{
	int s, pos = 0, done = 0;
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in myaddr_in, peeraddr_in;
	char *functionName = "WDOCUPDINV";
	char *service = "WDOCUPDCP";
	char c_HostName[HOSTNAMELENGTH+1] = {0};
	int lastChar;
	MAINTBUFFER buffer;
	DETAILS *details;
	char version[VERSIONLENGTH];

	signal(SIGALRM, TCPAlarm);

	strcpy(version, CLIENTVERSION);
	CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
		HOSTNAMELENGTH);

	/* 
		Zero the address structures. 
	*/
	memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset((char *)&peeraddr_in, 0, sizeof(struct sockaddr_in));

	details = (DETAILS *)parameters->details;

	peeraddr_in.sin_family = AF_INET;

	/* 
		Resolve the host information to a host in the /etc/hosts file. 
	*/
	hp = gethostbyname(c_HostName);
	if (hp == NULL) {
		strncpy(parameters->status, "01", 2);
		return;
	}

	peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	/* 
		Look up service information in the /etc/services file. 
	*/
	sp = getservbyname(service, "tcp");
	if (sp == NULL) {
		strncpy(parameters->status, "09", 2);
		return;
	}

	peeraddr_in.sin_port = sp->s_port;

	/* 
		Create the socket. 
	*/
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		strncpy(parameters->status, "03", 2);
		return;
	}

	alarm(30);
	/* 
		Connect to the remote service. 
	*/
	if (connect(s, &peeraddr_in, sizeof(struct sockaddr_in)) == -1) {
		strncpy(parameters->status, "02", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Send the version to the remote computer.
	*/
	alarm(30);
	if (!SendBuffer(NULL, s, version, VERSIONLENGTH)) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Send the data to the remote computer. 
	*/
	alarm(30);
	if (!SendBuffer(NULL, s, (char*)parameters, sizeof(FUNCPARMS))) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Receive data from remote computer.
	*/
	if (ReceiveData(s, (char*)parameters, sizeof(FUNCPARMS), 0) == -1) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}

	close(s);
}

/*
 ****************************************************************************
 *
 * Function:    TWDOCUPDINV
 *
 * Description: This fuction updates the inventory on WESDOC
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void TWDOCUPDCP(FUNCPARMS *parameters)
{
	int s, pos = 0, done = 0;
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in myaddr_in, peeraddr_in;
	char *functionName = "TWDOCUPDINV";
	char *service = "TWDOCUPDCP";
	char c_HostName[HOSTNAMELENGTH+1] = {0};
	int lastChar;
	MAINTBUFFER buffer;
	DETAILS *details;
	char version[VERSIONLENGTH];

	signal(SIGALRM, TCPAlarm);
	strcpy(version, CLIENTVERSION);

	CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
		HOSTNAMELENGTH);

	/* 
		Zero the address structures. 
	*/
	memset((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset((char *)&peeraddr_in, 0, sizeof(struct sockaddr_in));

	details = (DETAILS *)parameters->details;

	peeraddr_in.sin_family = AF_INET;

	/* 
		Resolve the host information to a host in the /etc/hosts file. 
	*/
	hp = gethostbyname(c_HostName);
	if (hp == NULL) {
		strncpy(parameters->status, "01", 2);
		return;
	}

	peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	/* 
		Look up service information in the /etc/services file. 
	*/
	sp = getservbyname(service, "tcp");
	if (sp == NULL) {
		strncpy(parameters->status, "09", 2);
		return;
	}

	peeraddr_in.sin_port = sp->s_port;

	/* 
		Create the socket. 
	*/
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		strncpy(parameters->status, "03", 2);
		return;
	}

	alarm(30);
	/* 
		Connect to the remote service. 
	*/
	if (connect(s, &peeraddr_in, sizeof(struct sockaddr_in)) == -1) {
		strncpy(parameters->status, "02", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Send the version to the remote computer.
	*/
	alarm(30);
	if (!SendBuffer(NULL, s, version, VERSIONLENGTH)) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Send the data to the remote computer. 
	*/
	alarm(30);
	if (!SendBuffer(NULL, s, (char*)parameters, sizeof(FUNCPARMS))) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}
	alarm(0);

	/* 
		Receive data from remote computer.
	*/
	if (ReceiveData(s, (char*)parameters, sizeof(FUNCPARMS), 0) == -1) {
		strncpy(parameters->status, "03", 2);
		close(s);
		return;
	}

	close(s);
}
