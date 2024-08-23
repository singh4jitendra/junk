#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/ttold.h>
#endif


#ifdef _GNU_SOURCE
#include <pty.h>
struct ncr_termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_cc[19];
};
#else
#include <termios.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <signal.h>

#ifndef _GNU_SOURCE
#include <sys/procset.h>
#endif

#include <libgen.h>

#if defined(_EMBEDDED_COBOL)
#   include "sub.h"
#endif    /* defined(_EMBEDDED_COBOL) */

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "gen_common_funcs.h"
#include "gen_server_funcs.h"

extern char *progName;
extern char *inifile;
extern int debugFile;
extern int db;

int genserver_get_file(wesco_socket_t s, LPPGPACKET lpPgPacket)
{
	ERRORPACKET epError;
	mode_t mFileMode;
	int nResult;

	if (debugFile)
		locallog(db, "INFO: PUT %s\n", lpPgPacket->filename);

	mFileMode = (mode_t)(ntohs(lpPgPacket->permissions));

	nResult = gs_get_file(s, lpPgPacket->filename, mFileMode, lpPgPacket->owner,
		lpPgPacket->group);
	if (nResult < 0) {
	   if (debugFile)
		   locallog(db, "INFO: PUT failed\n", lpPgPacket->filename);

		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "14", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	if (debugFile)
		locallog(db, "INFO: PUT successful\n");

	nResult = gs_send_ack_packet(s, _GEN_PUT);
	if (debugFile)
		locallog(db, "INFO: gs_send_ack_packet() = %d\n", nResult);

	return ((nResult == -1) ? -1 : 0);
}

int genserver_put_file(wesco_socket_t s, LPPGPACKET lpPgPacket)
{
	ERRORPACKET epError;
	mode_t mFileMode;
	char * lpOwner;
	char * lpGroup;
	int nResult;

	mFileMode = (mode_t)(ntohl(lpPgPacket->permissions));

	if (iscntrl((int)*(lpPgPacket->filename)) || isspace((int)*(lpPgPacket->filename)))
		return 0;

	lpOwner = ((*(lpPgPacket->owner)) == 0) ? NULL : lpPgPacket->owner;
	lpGroup = ((*(lpPgPacket->group)) == 0) ? NULL : lpPgPacket->group;

	nResult = gs_put_file(s, lpPgPacket->filename, mFileMode, lpOwner, lpGroup);
	if (nResult < 0) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "14", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	return (gs_send_ack_packet(s, _GEN_GET) == -1) ? -1 : 0;
}

int genserver_run(wesco_socket_t s, LPRUNPACKET lpRunPacket, char * lpUserDir, int heartbeat)
{
	GENTRANSAREA gtaParameters;
	ERRORPACKET epError;
	char * lpCobolArgv[4];
	int nCobolArgc;
   char szSwitches[SWITCHLENGTH+1];
	char * lpClientName;
	char * lpBaseDir;

#if defined(_EMBEDDED_COBOL)
	Argument aCobolArgs[1];
#endif

	if (debugFile)
      locallog(db, "INFO: RUN\n");

	if (ws_recv_packet(s, &gtaParameters, sizeof(gtaParameters)) <= 0) {
	   if (debugFile)
         locallog(db, "ERROR: Disconnect receiving GTA-TRANS-AREA\n");

		return -1;
	}

	strncpy(gtaParameters.status, "00", 2);
	if ((lpUserDir != NULL) && (*lpUserDir != 0)) {
		if ((lpBaseDir = strdup(lpUserDir)) == NULL)
			return -1;

		lpBaseDir = basename(lpBaseDir);
		strncpy(gtaParameters.partition, lpBaseDir, PARTITIONLENGTH);
	}

	if (debugFile)
		locallog(db, "INFO: Partition ID (in hex) = %x %x %x\n",
			gtaParameters.partition[0], gtaParameters.partition[1],
			gtaParameters.partition[2]);

#if defined(NSC)
	if ((lpClientName = calloc(20, sizeof(char))) == NULL) {
		return -1;
	}

	sprintf(lpClientName, "CLIENTNAME=x");
	strncpy(lpClientName+strlen(lpClientName), gtaParameters.clientname, 4);
	if (putenv(lpClientName) != 0) {
		return -1;
	}

	if (debugFile)
		locallog(db, "INFO: %s\n", lpClientName);
#endif

#if defined(_EMBEDDED_COBOL)
	aCobolArgs[0].a_address = (char *)&gtaParameters;
	aCobolArgs[0].a_length = sizeof(gtaParameters);

	lpCobolArgv[0] = progName;
locallog(db, "INFO: lpCobolArgv[0]=%s\n", lpCobolArgv[0]);
	if (*(gtaParameters.switches) == 32) {
		lpCobolArgv[1] = GENSERVER_COBOL_PROGRAM;
		nCobolArgc = 2;
locallog(db, "INFO: lpCobolArgv[1]=%s\n", lpCobolArgv[1]);
                lpCobolArgv[2] = lpCobolArgv[3] = NULL;
	}
	else {
		CobolToC(szSwitches, gtaParameters.switches, sizeof(szSwitches),
			sizeof(gtaParameters.switches));
		lpCobolArgv[1] = szSwitches;
		lpCobolArgv[2] = GENSERVER_COBOL_PROGRAM;
                lpCobolArgv[3] = NULL;
		nCobolArgc = 3;
locallog(db, "INFO: lpCobolArgv[1]=%s\n", lpCobolArgv[1]);
locallog(db, "INFO: lpCobolArgv[2]=%s\n", lpCobolArgv[2]);
	}
#endif

	if (lpRunPacket->redirect) {
#if defined(_EMBEDDED_COBOL)
		genserver_run_with_io_redirection(s, &gtaParameters, &nCobolArgc, lpCobolArgv,
			aCobolArgs, heartbeat);
#else
		genserver_run_with_io_redirection(s, &gtaParameters, NULL, NULL, NULL, heartbeat);
#endif
	}
	else {
#if defined(_EMBEDDED_COBOL)
		genserver_run_no_io_redirection(s, &gtaParameters, &nCobolArgc, lpCobolArgv,
			aCobolArgs);
#else
		genserver_run_no_io_redirection(s, &gtaParameters, NULL, NULL, NULL);
#endif
	}

	if (debugFile)
      locallog(db, "INFO: Sending GTA-TRANS-AREA\n");

	if (ws_send_packet(s, &gtaParameters, sizeof(gtaParameters)) <= 0)
		return -1;

	if (debugFile)
      locallog(db, "INFO: Sending ACK\n");

	return (gs_send_ack_packet(s, _GEN_RUN) == -1) ? -1 : 0;
}

int genserver_chdir(wesco_socket_t s, LPCDPACKET lpCdPacket)
{
	ERRORPACKET epError;

	if (debugFile)
      locallog(db, "INFO: CD %s\n", lpCdPacket->dirname);

	if (CheckDirectory(lpCdPacket->dirname) == DIRECTORY_ERROR) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "15", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	if (debugFile)
      locallog(db, "INFO: %s exists\n", lpCdPacket->dirname);

	if (ws_chdir(lpCdPacket->dirname) < 0) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "15", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	if (debugFile)
      locallog(db, "INFO: %s success\n", lpCdPacket->dirname);

	return (gs_send_ack_packet(s, _GEN_CD) == -1) ? -1 : 0;
}

int genserver_get_multiple_files(wesco_socket_t s, LPPGPACKET lpPgPacket)
{
	ERRORPACKET epError;

	if (gs_get_multiple_files(s) < 0) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "14", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	return (gs_send_ack_packet(s, _GEN_MPUT) == -1) ? -1 : 0;
}

int genserver_put_multiple_files(wesco_socket_t s, LPPGPACKET lpPgPacket)
{
	ERRORPACKET epError;

	if (!(*(lpPgPacket->filename)))
		return 0;

	if (gs_put_multiple_files(s, lpPgPacket->filename) < 0) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "14", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	return (gs_send_ack_packet(s, _GEN_MGET) == -1) ? -1 : 0;
}

int genserver_create_user_directory(wesco_socket_t s, LPPARTPACKET lpPartPacket,
												char * lpDirName)
{
	char szPartitionNumber[PARTITIONLENGTH+1];
	char szRenameDir[FILENAMELENGTH+1];
	ERRORPACKET epError;

	if (!GetPartition(szPartitionNumber, lpPartPacket->func, lpDirName)) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "11", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	sprintf(szRenameDir, "%s.%d", lpDirName, getpid());
	if (!rename(lpDirName, szRenameDir))
		strcpy(lpDirName, szRenameDir);

	if (chdir(lpDirName)) {
		memset(&epError, 0, sizeof(epError));
		strncpy(epError.status, "15", 2);
		epError.opcode = htons(_GEN_ERROR);
		epError.gerrno = htons(generr);

		ws_send_packet(s, &epError, sizeof(epError));

		return -1;
	}

	return (gs_send_ack_packet(s, _GEN_CRPART) == -1) ? -1 : 0;
}

int genserver_delete_user_directory(wesco_socket_t s, char * lpDirName)
{
	DeletePartition(lpDirName);

	return (gs_send_ack_packet(s, _GEN_DELPART) == -1) ? -1 : 0;
}

int genserver_run_with_io_redirection(wesco_socket_t s, LPGENTRANSAREA lpGtaParms,
												  int *pCobolArgc, char * lpCobolArgv[],
									           Argument *pCobolArgs,
												  int heartbeat)
{
#if defined _GNU_SOURCE
	struct ncr_termios netterm;
#endif

	struct termios tTerminal;
	struct winsize wWindowSize;
	int nBytesRead;
	int nFlags;
	short nPort;
	pid_t nChildPid;
	char szSlave[FILENAMELENGTH+1];
	char szCmdLine[FILENAMELENGTH+1];
	char szMessage[50];
	int nMaster;
	int nResult;
	int nStatus;
	wesco_socket_t io_s;
	caddr_t caGTA;
	int fdDevZero;
	int nTimeOut;
	extern struct sockaddr_in peeraddr_in;
#if defined(_CALL_COBOL)
	char * argv[4];
	char szSwitches[SWITCHLENGTH+1];
#endif

#if defined(MAP_ANON) || defined(MAP_ANONYMOUS)
/* Project 04041 - Linux does not define the MAP_VARIABLE macro */
# if defined(MAP_VARIABLE)
	caGTA = mmap(0, sizeof(GENTRANSAREA), PROT_READ|PROT_WRITE,
		MAP_ANONYMOUS|MAP_SHARED|MAP_VARIABLE, -1, 0);
# else
	caGTA = mmap(0, sizeof(GENTRANSAREA), PROT_READ|PROT_WRITE,
		MAP_ANONYMOUS|MAP_SHARED, -1, 0);
# endif
#else
	/* Open /dev/zero and map it to memory */
	if ((fdDevZero = open("/dev/zero", O_RDWR)) < 0) {
		return -1;
	}

	caGTA = mmap(0, sizeof(GENTRANSAREA), PROT_READ|PROT_WRITE, MAP_SHARED,
		fdDevZero, 0);
#endif

	if (caGTA == (caddr_t) -1) {
		close(fdDevZero);
		return -1;
	}

#if !defined(MAP_ANON) && !defined(MAP_ANONYMOUS)
	close(fdDevZero);
#endif

	memcpy(caGTA, lpGtaParms, sizeof(GENTRANSAREA));

	/* Get client terminal settings */
	if (ws_recv_packet(s, &netterm, sizeof(netterm)) <= 0)
		return -1;

#if defined(_GNU_SOURCE)
	tTerminal.c_iflag = ICRNL|IXON;
	tTerminal.c_oflag = OPOST|ONLCR|NL0|TAB3|BS0|FF0|VT0|CR0;
	tTerminal.c_cflag = B9600|CREAD|CS8;
	tTerminal.c_lflag = ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE|IEXTEN|ISIG|ICANON;
	tTerminal.c_cc[VINTR] = '';
	tTerminal.c_cc[VQUIT] = '';
	tTerminal.c_cc[VERASE] = '';
	tTerminal.c_cc[VKILL] = 0x15;
	tTerminal.c_cc[VEOF] = 0x04;
	tTerminal.c_cc[VSTART] = 0x11;
	tTerminal.c_cc[VSTOP] = 0x13;
	tTerminal.c_cc[VSUSP] = 0x1a;
	tTerminal.c_cc[VREPRINT] = 0x12;
	tTerminal.c_cc[VWERASE] = 0x17;
	tTerminal.c_cc[VLNEXT] = 0x16;
	tTerminal.c_cc[VMIN] = 1;
	tTerminal.c_cc[VTIME] = 0;
#else
	memset(&tTerminal, 0, sizeof(tTerminal));
	tTerminal.c_iflag = ntohl(netterm.c_iflag);
	tTerminal.c_oflag = ntohl(netterm.c_oflag);
	tTerminal.c_cflag = ntohl(netterm.c_cflag);
	tTerminal.c_lflag = ntohl(netterm.c_lflag);
	memcpy(tTerminal.c_cc, netterm.c_cc, 19);
#endif

#if defined(__hpux)
	tTerminal.c_reserved = ntohl(tTerminal.c_reserved);
#endif    /* defined(__hpux) */

	/* Get client window size */
	if (ws_recv_packet(s, &wWindowSize, sizeof(wWindowSize)) <= 0)
		return -1;

#if defined(_GNU_SOURCE)
	wWindowSize.ws_row = 24;
	wWindowSize.ws_col = 80;
	wWindowSize.ws_xpixel = 0;
	wWindowSize.ws_ypixel = 0;
#else
	wWindowSize.ws_row = ntohs(wWindowSize.ws_row);
	wWindowSize.ws_col = ntohs(wWindowSize.ws_col);
	wWindowSize.ws_xpixel = ntohs(wWindowSize.ws_xpixel);
	wWindowSize.ws_ypixel = ntohs(wWindowSize.ws_ypixel);
#endif

	if (debugFile)
		locallog(db, "INFO: receiving enviromental variables from client\n");

	/* Get environmental variables from client */
	while (1) {
		if ((nBytesRead = gs_recv_evar(s)) < 0)
			return -1;
		else if (nBytesRead == 1)
			break;
	}

	if (debugFile)
		locallog(db, "INFO: receiving port number from client\n");

	if (ws_recv_packet(s, &nPort, sizeof(nPort)) <= 0)
		return -1;

	nChildPid = pty_fork(&nMaster, szSlave, &tTerminal, &wWindowSize);
	switch (nChildPid) {
	case -1:
		break;
	case 0:
#if defined(_EMBEDDED_COBOL)
		nResult = acu_initv(*pCobolArgc, lpCobolArgv);
		if (nResult == 0)
			_exit(1);

		if (debugFile)
			locallog(db, "INFO: process %d on %s\n", getpid(), szSlave);

		pCobolArgs->a_address = (char *)caGTA;
		if (debugFile)
			locallog(db, "INFO: running ACU-COBOL\n");

		nResult = cobol(lpCobolArgv[((*(pCobolArgc)) - 1)], 1, pCobolArgs);
		if (nResult != 0) {
			strncpy(((LPGENTRANSAREA)caGTA)->status, GS_CALL_COBOL_ERROR, 2);

			acu_cancel(lpCobolArgv[((*(pCobolArgc)) - 1)]);
			acu_shutdown(1);
			_exit(1);
		}

		acu_cancel(lpCobolArgv[((*(pCobolArgc)) - 1)]);
		acu_shutdown(1);

		_exit(0);
#elif defined(_CALL_COBOL)
		/* close(s); */
		memset(szSwitches, 0, sizeof(szSwitches));
		CobolToC(szSwitches, lpGtaParms->switches, sizeof(szSwitches),
			sizeof(lpGtaParms->switches));
		sprintf(szCmdLine, "/usr/lbin/runcbl %s TRAFFICOP", szSwitches);
		if (debugFile)
			locallog(db, "INFO: %s\n", szCmdLine);

		argv[0] = "/usr/lbin/runcbl";
		argv[3] = NULL;
		if (*szSwitches == 0) {
			argv[1] = "TRAFFICOP";
			argv[2] = NULL;
		}
		else {
			argv[1] = szSwitches;
			argv[2] = "TRAFFICOP";
		}

		nResult = call_cobol(s, argv[0], argv, "t_linkage",
			(LPGENTRANSAREA)caGTA,
			sizeof(GENTRANSAREA));
		
		if (nResult != 0)
			_exit(1);
		else
			_exit(0);
#else
		close(s);
		memset(szCmdLine, 0, sizeof(szCmdLine));
		CobolToC(szCmdLine, lpGtaParms->hostprocess, sizeof(szCmdLine),
			sizeof(lpGtaParms->hostprocess));

		nResult = system(szCmdLine);
		if (nResult < 0)
			_exit(1);
		else
			_exit(0);
#endif    /* defined (_EMBEDDED_COBOL) */
		break;
	default:
		if (debugFile)
			locallog(db, "INFO: Parent GenServer is ready\n");

		peeraddr_in.sin_port = nPort;
		if ((io_s = ws_create_remote_connection(&peeraddr_in)) < 0)
			break;

		/* set the non-blocking I/O flag */
		nFlags = fcntl(s, F_GETFD);
		nFlags |= O_NONBLOCK;
		fcntl(s, F_SETFD, nFlags);

		/* set the non-blocking I/O flag */
		nFlags = fcntl(io_s, F_GETFD);
		nFlags |= O_NONBLOCK;
		fcntl(io_s, F_SETFD, nFlags);

		/* set the non-blocking I/O flag */
		nFlags = fcntl(nMaster, F_GETFD);
		nFlags |= O_NONBLOCK;
		fcntl(nMaster, F_SETFD, nFlags);

		memset(szCmdLine, 0, sizeof(szCmdLine));
		CobolToC(szCmdLine, lpGtaParms->hostprocess, sizeof(szCmdLine),
			sizeof(lpGtaParms->hostprocess));
		nTimeOut = GetPrivateProfileInt(GENSERVER_INI_SECTION, szCmdLine,
			0, inifile);
		if (debugFile)
			locallog(db, "INFO: Time-out for %s is %d\n", szCmdLine, nTimeOut);
		if (nTimeOut == 0 && heartbeat != 0) {
			nTimeOut = GetPrivateProfileInt(GENSERVER_INI_SECTION, "heartbeat",
				2, inifile);
		}
		else {
			heartbeat = 0;
		}

		pty_server_io(io_s, s, nMaster, nTimeOut, 0, heartbeat);

		/* clear the non-blocking I/O flag */
		nFlags = fcntl(s, F_GETFD);
		nFlags &= ~(O_NONBLOCK);
		fcntl(s, F_SETFD, nFlags);

		close(io_s);

		break;
	}

	alarm(30);
	nResult = (int)waitpid(nChildPid, &nStatus, 0);
	alarm(0);

	if (nResult > 0) {
		if (WIFEXITED(nStatus)) {
			if (WEXITSTATUS(nStatus) != 0) {
				sprintf(szMessage, "%03d", WEXITSTATUS(nStatus));
				strncpy(((LPGENTRANSAREA)caGTA)->status, "05", 2);
				strncpy(((LPGENTRANSAREA)caGTA)->error.name, "ret code", 8);
				strncpy(((LPGENTRANSAREA)caGTA)->error.description, szMessage, 2);
			}
		}
		else if (WIFSIGNALED(nStatus)) {
			sprintf(szMessage, "%02d", WTERMSIG(nStatus));
			strncpy(((LPGENTRANSAREA)caGTA)->status, "05", 2);
			strncpy(((LPGENTRANSAREA)caGTA)->error.name, "signal", 6);
			strncpy(((LPGENTRANSAREA)caGTA)->error.description, szMessage, 2);
		}
	}
	else {
		/* Child has not terminated, kill it!!! */
		strncpy(((LPGENTRANSAREA)caGTA)->status, "99", 2);
		strncpy(((LPGENTRANSAREA)caGTA)->error.name, "???????????????",
			ERRORNAMELENGTH);
		strncpy(((LPGENTRANSAREA)caGTA)->error.description, "???????????????",
			ERRORDESCLENGTH);
		kill(nChildPid, SIGTERM);
	}

	memcpy(lpGtaParms, caGTA, sizeof(GENTRANSAREA));
	munmap(caGTA, sizeof(GENTRANSAREA));

	if (debugFile)
		locallog(db, "INFO: GTA-RET-CODE=%c%c\n", lpGtaParms->status[0],
			lpGtaParms->status[1]);

	return 0;
}

int genserver_run_no_io_redirection(wesco_socket_t s, LPGENTRANSAREA lpGtaParameters,
												int *pCobolArgc, char * lpCobolArgv[],
												Argument *pCobolArgs)
{
	int nResult = 0x00FF;
	char szSwitches[SWITCHLENGTH+1];
	char szCmdLine[FILENAMELENGTH+1];
	char szMessage[256];
	char * argv[4];

	if (debugFile)
		locallog(db, "INFO: genserver_run_no_io_redirection();\n");

#if defined(_EMBEDDED_COBOL)
	nResult = acu_initv(*pCobolArgc, lpCobolArgv);
	if (!nResult) {
		if (debugFile)
			locallog(db, "ERROR: acu_initv() failed!  Check user count!\n");

		wescolog(NETWORKLOG, "ERROR: acu_initv() failed!\n");
		return -1;
	}

	if (debugFile)
		locallog(db, "INFO: Acu-COBOL initialized\n");

	if (cobol(GENSERVER_COBOL_PROGRAM, 1, pCobolArgs)) {
	   if (debugFile)
		   locallog(db, "INFO: Acu-COBOL failed: %d\n", return_code);

		strncpy(lpGtaParameters->status, "05", 2);
		wescolog(NETWORKLOG, "%s: %s %s, return_code = %d\n", progName,
			"error running COBOL program", GENSERVER_COBOL_PROGRAM, return_code);
		acu_cancel(GENSERVER_COBOL_PROGRAM);
		acu_shutdown(1);
		return -1;
	}

	if (debugFile)
		locallog(db, "INFO: Acu-COBOL completed\n");

	acu_cancel(GENSERVER_COBOL_PROGRAM);
	acu_shutdown(1);
#elif defined(_CALL_COBOL)
	memset(szSwitches, 0, sizeof(szSwitches));
	CobolToC(szSwitches, lpGtaParameters->switches, sizeof(szSwitches),
		sizeof(lpGtaParameters->switches));
	sprintf(szCmdLine, "/usr/lbin/runcbl %s TRAFFICOP", szSwitches);
	if (debugFile)
		locallog(db, "INFO: %s\n", szCmdLine);

	argv[0] = "/usr/lbin/runcbl";
	argv[3] = NULL;
	if (*szSwitches == 0) {
		argv[1] = "TRAFFICOP";
		argv[2] = NULL;
	}
	else {
		argv[1] = szSwitches;
		argv[2] = "TRAFFICOP";
	}

	nResult = call_cobol(s, argv[0], argv, "t_linkage", lpGtaParameters,
		sizeof(GENTRANSAREA));

	if (nResult != 0) {
		sprintf(szMessage, "%d", nResult);
		strncpy(lpGtaParameters->status, "05", 2);
		strncpy(lpGtaParameters->error.name, "code", 4);
		strncpy(lpGtaParameters->error.description, szMessage, strlen(szMessage));
		return -1;
	}

	if (debugFile)
		locallog(db, "INFO: GTA-RET-CODE=%c%c\n", lpGtaParameters->status[0],
			lpGtaParameters->status[1]);
#else
	memset(szCmdLine, 0, sizeof(szCmdLine));
	CobolToC(szCmdLine, lpGtaParameters->hostprocess, sizeof(szCmdLine),
		sizeof(lpGtaParameters->hostprocess));

	nResult = system(szCmdLine);
	return (nResult < 0) ? -1 : 0;
#endif

	return 0;
}

int gs_check_login(wesco_socket_t s, LPPWDPACKET lpPwdPacket)
{
	FILE *fpPwdFile;
	char * lpEncryptedPassword;
	char szBuffer[80];
	char szSalt[6];
	char *pToken;
	struct passwd *pPwdEntry;
	struct ws_pfent *pfEntry;
	time_t tCurrTime;
	struct tm *lpCurrDay;
	int32_t nCurrJul;

	if ((pfEntry = ws_getpfname(lpPwdPacket->username)) == NULL) {
		if (debugFile)
			locallog(db, "ERROR: Login failed!\n");

		gs_send_error_packet(s, "19", "usr\\pwd");
		return -1;
	}

	/* check the password */
	memset(szSalt, 0, sizeof(szSalt));
	strncpy(szSalt, pfEntry->pf_passwd, 5);
	lpEncryptedPassword = crypt(lpPwdPacket->password, szSalt);

	if (strcmp(pfEntry->pf_passwd, lpEncryptedPassword) != 0) {
		if (debugFile)
			locallog(db, "ERROR: Login failed on Password!\n");

		gs_send_error_packet(s, "19", "usr\\pwd");
		return -1;
	}

	/* check to ensure user is in /etc/passwd */
	if ((pPwdEntry = getpwnam(lpPwdPacket->username)) == NULL) {
		if (debugFile)
			locallog(db, "ERROR: Login failed on /etc/passwd!\n");

		gs_send_error_packet(s, "19", "usr\\pwd");
		return -1;
	}
	else {
		setgid(pPwdEntry->pw_gid);
		setuid(pPwdEntry->pw_uid);
	}

	/* is the password expired or locked */
	tCurrTime = time(NULL);
	lpCurrDay = gmtime(&tCurrTime);
	nCurrJul = (lpCurrDay->tm_year * 365) + lpCurrDay->tm_yday;

	if ((pfEntry->pf_attempts > 7) ||
		 ((pfEntry->pf_lastaccess + 30) < nCurrJul)) {
		if (debugFile)
			locallog(db, "ERROR: Login failed on lock check!\n");

		gs_send_error_packet(s, "19", "locked");
		return -1;
	}
	
	if ((pfEntry->pf_lastchange + 90) < nCurrJul)  {
		if (debugFile)
			locallog(db, "ERROR: Login failed on password expired!\n");

		gs_send_error_packet(s, "19", "passwd");
		return -1;
	}

	if (debugFile)
		locallog(db, "Login successful!\n");

	return gs_send_ack_packet(s, _GEN_LOGIN);
}

bool_t gs_in_ip_family(char * lpDbName, char * lpHost)
{
	FILE *fp;
	char szBuffer[256];
	char szIP[256];

	if ((fp = fopen(lpDbName, "r")) == NULL) {
		return FALSE;
	}

	while (!feof(fp)) {
		if (fgets(szBuffer, sizeof(szBuffer), fp) == NULL) {
			fclose(fp);
			return FALSE;
		}

		if (*szBuffer == '#') /* comment */
			continue;

		sscanf(szBuffer, "%15s", szIP);

		if (debugFile)
			locallog(db, "INFO: Checking %s against %s\n", lpHost, szIP);

		if (!strncmp(lpHost, szIP, strlen(szIP))) {
			fclose(fp);
			return TRUE;
		}
	}

	fclose(fp);
	return FALSE;
}

#if 0
FILE *ws_openpffile(char *lpFileName)
{
	int nRetries = 0;
	FILE *fpPwdFile = (FILE *)NULL;

	while (symlink(WS_PASSWD_FILE, WS_PASSWD_TEMP) != 0) {
		sleep(1);
		if ((nRetries++) > 30)
			return (FILE *)NULL;
	}

	if ((fpPwdFile = fopen(lpFileName, "r+")) == (FILE *)NULL) {
		unlink(WS_PASSWD_TEMP);
		return (FILE *)NULL;
	}

	return fpPwdFile;
}
#endif

struct ws_pfent *ws_getpfname(char * lpName)
{
	static struct ws_pfent pfEntry;
	char szBuffer[1024];
	char szYear[3];
	int32_t nYear;
	FILE *fpPwdFile;
	char *pToken;

	if ((fpPwdFile = fopen(WS_PASSWD_FILE, "r")) == (FILE *)NULL)
		return (struct ws_pfent *)NULL;

	while (!feof(fpPwdFile)) {
		fgets(szBuffer, sizeof(szBuffer), fpPwdFile);
		if ((pToken = strtok(szBuffer, ":\n")) == NULL)
			continue;

		if (debugFile)
			locallog(db, "INFO: checking %s and %s\n", pToken, lpName);

		if (!strcmp(pToken, lpName)) {
			strcpy(pfEntry.pf_name, pToken);

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing password\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: password = %s\n", pToken);

				strcpy(pfEntry.pf_passwd, pToken);
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing start shell\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: start shell = %s\n", pToken);

				strcpy(pfEntry.pf_start, pToken);
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing language code\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: language code = %c\n", *pToken);

				pfEntry.pf_langcode = *pToken;
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing last access date\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: last access date = %d\n", pToken);

				memset(szYear, 0, sizeof(szYear));
				strncpy(szYear, pToken, 2);
				nYear = atol(szYear);
				if (nYear < 50)
					nYear += 100;

				pfEntry.pf_lastaccess = (nYear * 365) + atol(pToken+2);
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing last change date\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: last change date = %d\n", pToken);

				memset(szYear, 0, sizeof(szYear));
				strncpy(szYear, pToken, 2);
				nYear = atol(szYear);
				if (nYear < 50)
					nYear += 100;

				pfEntry.pf_lastchange = (nYear * 365) + atol(pToken+2);
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: entry missing attempts\n");

				continue;
			}
			else {
				if (debugFile)
					locallog(db, "INFO: number of attempts = %d\n", atol(pToken));

				pfEntry.pf_attempts = atol(pToken);
			}

			if ((pToken = strtok(NULL, ":\n")) == NULL) {
				if (debugFile)
					locallog(db, "ERROR: defaulting supervisor flag to ' '\n");

				pfEntry.pf_sflag = ' ';
			}
			else {
				if (debugFile)
					locallog(db, "INFO: supervisor flag = %c\n", *pToken);

				pfEntry.pf_sflag = *pToken;
			}

			fclose(fpPwdFile);

			return &pfEntry;
		}
	}

	fclose(fpPwdFile);

	return (struct ws_pfent *)NULL;
}
