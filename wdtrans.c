#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>

#ifdef _GNU_SOURCE 
#include <dirent.h>
#else
#include <sys/dirent.h>
#endif

#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "receive.h"
#include "queue.h"

#ifndef MAXRECEIVES
#define MAXRECEIVES	  10
#endif

#ifndef NETWORKLOG
#define NETWORKLOG -1
#endif

#define TRANSMITTED	   1
#define TRANSMITFAIL	   0
#define CONNECTED		   0
#define CONNECTFAIL	   1
#define LOGFILE			"/IMOSDATA/5/wdtransmit.log"
#define PIDFILE			"/IMOSDATA/5/wdtransmit.pid"
#define DEBUGSERVER
#ifndef WESCOMINI
#define WESCOMINI     "/IMOSDATA/2/wescom.ini"
#endif

int s, ls;

struct hostent *hp;
struct servent *sp;

struct linger linger;
struct sockaddr_in myaddr_in, peeraddr_in;

int db;
int debugfile = 0;
int logbuffer = 0;
int debugsocket = 0;

#ifndef TESTSERVER
char *sendVar = "WDOCQDIR";
#else
char *sendVar = "TWDOCQDIR";
#endif

char *progName = 0;

void DoForever(char *argv[]);
void ProcessDirectory(char *, char *);
int Transmit(char *);
int ConnectToRemote(char *);
void delete_pid_file(void);

main(int argc, char *argv[])
{
	int retCode;
	int option;
	mode_t oldmask;

	progName = argv[0];

	/* set file creation mask */
	oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

	/* process command line options */
	while ((option = getopt(argc, argv, "bfs")) != EOF) {
		switch (option) {
			case 'b':
				logbuffer = 1;
				break;
			case 'f':
				debugfile = 1;
				break;
			case 's':
				debugsocket = 1;
				break;
			default:
				exit(1);
		}
	}

	if (debugfile) {
		if ((db = creat("debug.wdt", 0666)) < 0) {
			fprintf(stderr, "%s: Can't open debug file\n", progName);
			debugfile = FALSE;
		}
	}

	if (debugfile)
		wescolog(db, "initializing the daemon\n");

	daemoninit();

	if (debugfile)
		wescolog(db, "the daemon is initialized\n");

	memset((char *)&myaddr_in, 0, sizeof(myaddr_in));
	memset((char *)&peeraddr_in, 0, sizeof(peeraddr_in));

	if (check_pid_file(PIDFILE)) {
		fprintf(stderr, "wdtransmit: only one is allowed to run at a time!!!\n");
		exit(1);
	}

	atexit(delete_pid_file);

	setsignal(SIGPIPE, SIG_IGN, NULL, 0, 0);
	setsignal(SIGHUP, SIG_IGN, NULL, 0, 0);
	DoForever(argv);
}

/*
 ****************************************************************************
 *
 * Function:	 DoForever
 *
 * Description: This function sits in an infinite loop and processes the
 *				  queue directories.  It waits until it is wakened by the
 *				  semaphore by being incremented.
 *
 * Parameters:  argv - The command line arguments.
 *
 * Returns:	  Nothing
 *
 ****************************************************************************
*/
void DoForever(char *argv[])
{
	char *hostName, *tempQ, buffer[BUFFERSIZE];
	int totalBytes = 0, bytesReceived = 0, pos = 0;
	char QPath[FILENAMELENGTH], currFilename[FILENAMELENGTH];
	int retVal;
    int sleepTimer;
	DIR *dir;
	struct dirent *entry;

	/* clear the character buffer */
	memset(buffer, 0, sizeof(buffer));
	memset(currFilename, 0, sizeof(currFilename));

	/* search for the queue directory environmental variable */
	tempQ = getenv(sendVar);
	if (tempQ == NULL) {
		wescolog(NETWORKLOG,
			"%s: %s variable not defined!  Server Terminated!", progName,
			sendVar);
		exit(1);
	}

	strcpy(QPath, tempQ);

	/*
		Sit in an infinite loop.  Wait for the semaphore to equal zero.  If
		it equals zero than a file is waiting to be transmitted.
	*/
	while (1) {
        sleepTimer=GetPrivateProfileInt("wdtransmit", "sleepTimer", 120, WESCOMINI);
		sleep(sleepTimer);
 
		/* open the Queue root directory */
		if ((dir = opendir(QPath)) == NULL) {
			wescolog(NETWORKLOG, "%s: can not open directory %s: %s",
				progName, QPath, strerror(errno));
			exit(1);
		}

		/* 
			Each entry in the Queue root directory should be a directory
			that corresponds to a known host.  The only exception being
			the seqnum file.  Open the directory and process it.
		*/
		while ((entry = readdir(dir)) != NULL) {
			strcpy(currFilename, QPath);
			pos = strlen(currFilename);
			if (currFilename[pos-1] != '/') {
				currFilename[pos] = '/';
				currFilename[pos+1] = 0;
			}

			strcat(currFilename, entry->d_name);

			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
				ProcessDirectory(currFilename, entry->d_name);	
		}

		closedir(dir);
	}
}

/*
 ****************************************************************************
 *
 * Function:	 ProcessDirectory
 *
 * Description: This function processes each directory and sends the
 *				  .Q files to the remote computer.
 *
 * Parameters:  dirName  - The name of the directory to process.
 *				  hostName - The name of the host to which to connect.
 *
 * Returns:	  Nothing
 *
 ****************************************************************************
*/
void ProcessDirectory(char *dirName, char *hostName)
{
	int pos, connected = FALSE;
	char dataFilename[FILENAMELENGTH+1], newFilename[FILENAMELENGTH+1];
	char qFilename[FILENAMELENGTH+1];
	DIR *dir;
	struct dirent *entry;
	int test;

	if ((dir = opendir(dirName)) == NULL)
		return;

	/*
		Check each entry in the directory specified by dirName.  The .Q files
		will be transmitted along with their corresponding .dat files.
		Keep the tcp connection open until the last file has been 
		transmitted.  The closing of the connection will let the remote
		computer know that all files have been transmitted.
	*/
	while ((entry = readdir(dir)) != NULL) {
		if (debugfile) {
			wescolog(db, "file -> %s\n", entry->d_name);
			wescolog(db, "hostname -> %s\n", hostName);
		}

		if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
			strcpy(qFilename, dirName);
			strcat(qFilename, "/");
			strcat(qFilename, entry->d_name);
			pos = strlen(qFilename) - 2;
			if (!strcmp(&qFilename[pos], ".Q")) {
				if (!connected) {
					if (ConnectToRemote(hostName) == CONNECTED)
						connected = TRUE;
					else 
						connected = FALSE;
				}

				if (connected) {
					/* transmit files */
					if (Transmit(qFilename) == TRANSMITFAIL) {
						wescolog(NETWORKLOG, "%s: can not transmit to %s",
							progName, hostName);
						closedir(dir);
						close(s);
						return;
					}

					/* create file names */
					strcpy(dataFilename, qFilename);
					strcpy(newFilename, qFilename);
					pos = strlen(newFilename) - 1;
					newFilename[pos] = 'P';
					pos = strlen(dataFilename) - 1;
					dataFilename[pos] = 0;
					strcat(dataFilename, "dat");

					/* test for the data file */
					if ((test = open(dataFilename, O_RDONLY)) > -1) {
						close(test);
						if (Transmit(dataFilename) == TRANSMITFAIL) {
							wescolog(NETWORKLOG, "%s: error transmitting %s\n",
								progName, dataFilename);

							if (debugfile)
								wescolog(db, "error transmitting data file\n");

							close(s);
							closedir(dir);
							return;
						}
					}

					rename(qFilename, newFilename);
				}
				else
					break;
			}
		}
	}

	if (connected) close(s);

	closedir(dir);

	return;
}

/*															  
 ****************************************************************************
 *
 * Function:	 Transmit
 *
 * Description: This function transmits the specified file to the remote
 *				  computer.
 *
 * Parameters:  filename  - The name of the file to transmit.
 *
 * Returns:	  TRANSMIT or TRANSMITFAIL.
 *
 ****************************************************************************
*/
int Transmit(char *filename)
{
	char buffer[1024];
	int pos, transFile, done = FALSE, bytesRead;
	int lb;
 
	memset(buffer, 0, sizeof(buffer));

	if (debugfile)
		wescolog(db, "Transmitting file -> %s\n", filename);

	/* Open the file that is to be transmitted */
	if ((transFile = open(filename, O_RDONLY)) < 0) {
		wescolog(NETWORKLOG, "%s: error opening file %s: %s\n",
			progName, filename, strerror(errno));
		return TRANSMITFAIL;
	}

	/* send file in 1024b increments */
	while (!done) {
		bytesRead = read(transFile, buffer, sizeof(buffer));
		if (bytesRead < 1024 && bytesRead > -1) {
			done = TRUE;
			buffer[bytesRead] = 127;
		}
		else if (bytesRead == -1) {
			wescolog(NETWORKLOG, "%s: transfer of %s failed: %s\n",
				progName, filename, strerror(errno));
			close(s);
			close(transFile);
			return TRANSMITFAIL;
		}

		if (debugfile)
			wescolog(db, "sending %d bytes\n", bytesRead);

		if (logbuffer) {
			if ((lb = open(LOGFILE, O_CREAT|O_WRONLY|O_APPEND, 0666)) > -1) {
				wescolog(lb, "*** %s ***\n", filename);
				write(lb, buffer, sizeof(buffer));
				write(lb, "\n\n", 2);
				close(lb);
			}
		}

		if (send(s, buffer, sizeof(buffer), 0) != sizeof(buffer)) {
			wescolog(NETWORKLOG,
				"%s: connection terminated while transmitting: %s", progName,
				strerror(errno));
			close(s);
			close(transFile);
			return TRANSMITFAIL;
		}
	}

	/* Receive OK from the remote computer */
	if (ReceiveData(s, buffer, sizeof(char) * 2, 0) <= 0) {
		wescolog(NETWORKLOG,
			"%s: connection terminated receiving OK: %s", progName,
			strerror(errno));
		close(s);
		close(transFile);
		return TRANSMITFAIL;
	}

	close(transFile);

	return TRANSMITTED;
}

/*															  
 ****************************************************************************
 *
 * Function:	 ConnectToRemote
 *
 * Description: This function connects to the specified remote computer.
 *
 * Parameters:  c_HostName  - The name of the host to which to connect.
 *
 * Returns:	  CONNECTED if connected, else CONNECTFAIL.
 *
 ****************************************************************************
*/
int ConnectToRemote(char *c_HostName)
{
	char *service = "WDOCRECV";

	peeraddr_in.sin_family = AF_INET;

	/* resolve host name to entry in /etc/hosts file */
	if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
		wescolog(NETWORKLOG, "%s: %s not found in /etc/hosts", 
			progName, c_HostName);
		return CONNECTFAIL;
	} 

	peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	/* resolve service to entry in /etc/services file */
	if ((sp = getservbyname(service, "tcp")) == NULL) {
		wescolog(NETWORKLOG, "%s: %s not found in /etc/services", 
			progName, service);
		return CONNECTFAIL;
	}

	peeraddr_in.sin_port = sp->s_port;

	/* create the socket */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		wescolog(NETWORKLOG, "%s: unable to create socket: %s\n",
			progName, strerror(errno));
		return CONNECTFAIL;
	}

	/* connect to remote computer */
	if (connect(s, (struct sockaddr *)&peeraddr_in, sizeof(peeraddr_in)) < 0) {
		wescolog(NETWORKLOG, "%s: error connecting to remote: %s\n", 
			progName, strerror(errno));
		close(s);

		return CONNECTFAIL;
	}

	if (debugsocket)
		setsocketdebug(s, 1);

	if (debugfile)
		wescolog(db, "Connected to %s, %d\n", inet_ntoa(peeraddr_in.sin_addr),
			peeraddr_in.sin_port);

	return CONNECTED;
}

void delete_pid_file(void)
{
	remove(PIDFILE);
}
