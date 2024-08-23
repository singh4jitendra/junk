#if defined(_WIN32)
#   include <windows.h>
#   include "resource.h"
#endif      /* defined(_WIN32) */

#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32)
#   include <sys/time.h>
#   include <sys/socket.h>
#   include <sys/stat.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#   include <sys/ttold.h>
#endif


#   if !defined(__hpux) && !defined(_GNU_SOURCE)
#      include <sys/sockio.h>
#   endif    /* !defined(__hpux) */

#   include <netinet/in.h>
#   include <unistd.h>
#   include <signal.h>
#   include <netdb.h>
#   include <fcntl.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#   include <termios.h>
#endif

#   include <libgen.h>
#   include <dirent.h>
#   include <limits.h>

#   include "msdefs.h"
#endif       /* !defined(_WIN32) */

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "gen_client_funcs.h"
#include "gen_common_funcs.h"
#include "ws_syslog.h"

#define GETACK(x, y)      if (client_get_ack(x, y)) return -1;

#if defined(_WIN32)
#   include "login.h"
extern wesco_handle_t ghMod;
#else
int queuetransaction(int, bool_t, GENTRANSAREA *);
int sendevar(int, char *);
#endif

char gin[1024];
char gout[1024];

int GenClient(LPGENTRANSAREA parameters)
{
	wesco_socket_t s = -1;
	char opModeStr[OPMODELENGTH+1];
	int  opMode;
	short done = htons(_GEN_DONE);
	int login_attempt;
	int32_t gen_flags;
	GENVERSION gVersion;
	int nResult;
	LPFNGENFUNC lpfnDoGenCommands;
	int heartbeat;

#if defined(_WIN32)
	HCURSOR hWaitCursor;
	HCURSOR hArrowCursor;
	HINSTANCE hGenDll;
#endif

	login_attempt = 1;

	memset(opModeStr, 0, sizeof(opModeStr));
	strncpy(opModeStr, parameters->opmode, 2);
	opMode = atol(opModeStr);

#if defined(_WIN32)
	hWaitCursor = LoadCursor(NULL, IDC_WAIT);
	hArrowCursor = LoadCursor(NULL, IDC_ARROW);
#endif

	if (!IS_QUEUED_MODE(opMode)) {
      if ((s = ConnectToRemote(parameters)) == SOCKET_ERROR) {
         wesco_closesocket(s);
			return -1;
      }

      if (!NegotiateVersion(s, &gVersion, parameters)) {
			wesco_closesocket(s);
			return -1;
      }

#if !defined(_FIRST_GO) && defined(_WIN32)
      if ((hGenDll = LoadLibrary(gVersion.dllpath)) == NULL) {
         strncpy(parameters->status, "98", (size_t)2);
			wesco_closesocket(s);
         return -1;
      }

      lpfnDoGenCommands = (LPFNGENFUNC)GetProcAddress(hGenDll,
         gVersion.function);
      if (lpfnDoGenCommands == NULL) {
         strncpy(parameters->status, "99", (size_t)2);
			wesco_closesocket(s);
         return -1;
      }
#else
	  lpfnDoGenCommands = (LPFNGENFUNC)gVersion.client_function;
	  heartbeat = gVersion.heartbeat;
#endif

#if !defined(_FIRST_GO) && defined(_WIN32)
      if (gs_login(s, parameters) < 0) {
         strncpy(parameters->status, "19", (size_t)2);
   		strncpy(parameters->error.name, "login", (size_t)5);
			wesco_closesocket(s);
         return -1;
      }
#endif
	}

	ws_syslog(LOG_DEBUG, "GTA-OP-MODE=%02d", opMode);

	switch (opMode) {
	case SPECIFIEDPART_TRANS:
		gen_flags = GEN_SEND_CD|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_INFILE:
		gen_flags = GEN_SEND_CD|GEN_SEND_FILE|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_OUTFILE:
		gen_flags = GEN_SEND_CD|GEN_SEND_RUN|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_IOFILES:
		gen_flags = GEN_SEND_CD|GEN_SEND_FILE|GEN_SEND_RUN|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_MULTIN:
		gen_flags = GEN_SEND_CD|GEN_SEND_MFILE|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_MULTOUT:
		gen_flags = GEN_SEND_CD|GEN_SEND_RUN|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case SPECIFIEDPART_MULTIO:
		gen_flags = GEN_SEND_CD|GEN_SEND_MFILE|GEN_SEND_RUN|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_TRANS:
		gen_flags = GEN_USER_PART|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_INFILE:
		gen_flags = GEN_USER_PART|GEN_SEND_FILE|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_OUTFILE:
		gen_flags = GEN_USER_PART|GEN_SEND_RUN|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_IOFILES:
		gen_flags = GEN_USER_PART|GEN_SEND_FILE|GEN_SEND_RUN|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_MULTIN:
		gen_flags = GEN_USER_PART|GEN_SEND_MFILE|GEN_SEND_RUN;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_MULTOUT:
		gen_flags = GEN_USER_PART|GEN_SEND_RUN|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case CREATEDPART_MULTIO:
		gen_flags = GEN_USER_PART|GEN_SEND_MFILE|GEN_SEND_RUN|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_SEND:
		gen_flags = GEN_SEND_CD|GEN_SEND_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_RETRIEVE:
		gen_flags = GEN_SEND_CD|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_SENDRETRIEVE:
		gen_flags = GEN_SEND_CD|GEN_SEND_FILE|GEN_RECV_FILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_MULTISEND:
		gen_flags = GEN_SEND_CD|GEN_SEND_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_MULTIRETRIEVE:
		gen_flags = GEN_SEND_CD|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
	case FILE_MULTISENDRETRIEVE:
		gen_flags = GEN_SEND_CD|GEN_SEND_MFILE|GEN_RECV_MFILE;
		nResult = lpfnDoGenCommands(s, &gen_flags, parameters, heartbeat);
		break;
#if !defined(_WIN32)
	case GQ_SPECIFIEDPARTITION:
	case GQ_CREATEDPARTITION:
		queuetransaction(opMode, FALSE, parameters);
		break;
	case GQ_SPECIFIEDPART_INFILE:
	case GQ_CREATEDPART_INFILE:
	case GQ_SPECIFIEDPART_MULTIN:
	case GQ_CREATEDPART_MULTIN:
	case GQ_FILE_SEND:
	case GQ_FILE_MULTISEND:
		queuetransaction(opMode, TRUE, parameters);
		break;
#endif
	default:
		strncpy(parameters->status, "18", 2);
		nResult = 18;
#if !defined(_FIRST_GO) && defined(_WIN32)
      FreeLibrary(hGenDll);
#endif
		wesco_closesocket(s);
		return -1;
	};

	if (!IS_QUEUED_MODE(opMode)) {
   	ws_send_packet(s, &done, sizeof(done));
   	wesco_closesocket(s);
	}

#if !defined(_FIRST_GO) && defined(_WIN32)
   FreeLibrary(hGenDll);
#endif
	if (s != -1)
		wesco_closesocket(s);

   return nResult;
}

#if !defined(_WIN32)
int queuetransaction(int opmode, bool_t sendfiles, GENTRANSAREA *parameters)
{
   char opmodestr[OPMODELENGTH+1];
   char qfilename[FILENAMELENGTH];
   char qprefix[FILENAMELENGTH];
   char queuedir[FILENAMELENGTH];
   char queuetxfile[FILENAMELENGTH];
   char txfile[FILENAMELENGTH];
   char txdir[FILENAMELENGTH];
   char temp[FILENAMELENGTH];
   char newname[FILENAMELENGTH];
   char fullname[FILENAMELENGTH];
   DIR *dirp;
   struct dirent *entry;
   int qfd;
   int count;
   int found = 0;

   opmode -= 20;
   sprintf(opmodestr, "%02d", opmode);
   CtoCobol(parameters->opmode, opmodestr, sizeof(parameters->opmode));

	 strncpy(parameters->status, "00", 2);

   /* read INI files */
   GetPrivateProfileString("genserver", "queuedir", queuedir, sizeof(queuedir),
      "/IMOSDATA/2/QUEUES/GQDIR", WESCOMINI);
   GetPrivateProfileString("genserver", "queueprefix", qprefix,
      sizeof(qprefix), "genqueue", WESCOMINI);

   /* add trailing '/' if necessary */
   count = strlen(queuedir) - 1;
   if (*(queuedir + count) != '/' && count < FILENAMELENGTH - 1)
      strcat(queuedir, "/");

   strcpy(qfilename, queuedir);
   CobolToC(qfilename+strlen(qfilename), parameters->hostname, 
      sizeof(parameters->hostname)+1, sizeof(parameters->hostname));
   strcat(qfilename, "/");
   CheckDirectory(qfilename);

   memset(parameters->clientname, 32, sizeof(parameters->clientname));
   strncpy(parameters->clientname, "GQXMIT", 6);

   getqueuefilename(qfilename+strlen(qfilename), qprefix);

   if ((qfd = creat(qfilename, READWRITE)) < 0) {
		ws_syslog(LOG_ERR, "creat(%s): %s", qfilename, strerror(errno));
      strncpy(parameters->status, "08", 2);
      return -1;
   }

   if (sendfiles) {
      CobolToC(temp, parameters->clientsendfile, sizeof(temp),
         sizeof(parameters->clientsendfile));
      ParseFilename(temp, txdir, txfile);
      if ((dirp = opendir(txdir)) == NULL) {
			ws_syslog(LOG_ERR, "opendir(%s): %s", txdir, strerror(errno));
         CtoCobol(parameters->status, "08", sizeof(parameters->status));
         CtoCobol(parameters->error.name, "opening",
            sizeof(parameters->error.name));
         CtoCobol(parameters->error.description, txdir,
            sizeof(parameters->error.description));
			close(qfd);
			remove(qfilename);
         return -1;
      }

      while ((entry = readdir(dirp)) != NULL) {
         if (file_pattern_match(entry->d_name, txfile)) {
            sprintf(fullname, "%s/%s", txdir, entry->d_name);
            sprintf(queuetxfile, "%s.%s", qfilename, entry->d_name);
            if (!CopyFile(fullname, queuetxfile)) {
					ws_syslog(LOG_ERR, "%s: could not copy file", fullname);
               CtoCobol(parameters->status, "08", sizeof(parameters->status));
               CtoCobol(parameters->error.name, "copying",
                  sizeof(parameters->error.name));
               CtoCobol(parameters->error.description, entry->d_name,
                  sizeof(parameters->error.description));
					closedir(dirp);
					close(qfd);
					remove(qfilename);
               return -1;
            }
            found = 1;
         }
      }

		closedir(dirp);

      if (!found) {
			ws_syslog(LOG_ERR, "%s: could not find", txfile);
         CtoCobol(parameters->status, "08", sizeof(parameters->status));
         CtoCobol(parameters->error.name, "not found",
            sizeof(parameters->error.name));
         CtoCobol(parameters->error.description, temp,
            sizeof(parameters->error.description));
			close(qfd);
			remove(qfilename);
         return -1;
      }

      sprintf(queuetxfile, "%s.%s", qfilename, txfile);
      memset(parameters->clientsendfile, 32,
         sizeof(parameters->clientsendfile));
      CtoCobol(parameters->clientsendfile, queuetxfile,
         sizeof(parameters->clientsendfile));
   }

   count = sizeof(GENTRANSAREA);
   if (writen(qfd, parameters, count) < count) {
		ws_syslog(LOG_ERR, "could not write to gendata file");
      CtoCobol(parameters->status, "08", sizeof(parameters->status));
      CtoCobol(parameters->error.name, "genqueue",
         sizeof(parameters->error.name));
      CtoCobol(parameters->error.description, "write error",
         sizeof(parameters->error.description));
   }

   close(qfd);

   strcpy(newname, qfilename);
   strcat(newname, ".gendata");

   rename(qfilename, newname);

   return 0;
}
#endif

int WINAPI do_genserver_commands(wesco_socket_t s, int32_t * gen_flags, LPGENTRANSAREA parms, int heartbeat)
{
	ws_syslog(LOG_DEBUG, "do_genserver_commands()");

	if (*gen_flags & GEN_SEND_CD) {
		CobolToC(gout, parms->hostarea, sizeof(gout), sizeof(parms->hostarea));
		ws_syslog(LOG_DEBUG, "CD <%s>", gout);
		if (gs_send_cd_packet(s, gout) < 0) {
			strncpy(parms->status, "18", 2);
			return -1;
		}

		GETACK(s, parms);
	}
	else if (*gen_flags & GEN_USER_PART) {
		CobolToC(gout, parms->hostarea, sizeof(gout), sizeof(parms->hostarea));
		ws_syslog(LOG_DEBUG, "CRPART <%s>", gout);
		if (gs_send_pp_packet(s, _GEN_CRPART, gout) < 0) {
			strncpy(parms->status, "18", 2);
			return -1;
		}

		GETACK(s, parms);
	}

	if (*gen_flags & GEN_SEND_FILE) {
		CobolToC(gout, parms->clientsendfile, sizeof(gout),
			sizeof(parms->clientsendfile));
		ws_syslog(LOG_DEBUG, "PUT <%s>", gout);
		if (client_put_file(s, parms) < 0) {
			strncpy(parms->status, "16", 2);
			return -1;
		}

		GETACK(s, parms);
	}
	else if (*gen_flags & GEN_SEND_MFILE) {
		CobolToC(gout, parms->clientsendfile, sizeof(gout), sizeof(parms->clientsendfile));
		ws_syslog(LOG_DEBUG, "MPUT <%s>", gout);

		if (client_put_multiple_files(s, parms) < 0) {
			strncpy(parms->status, "16", 2);
			return -1;
		}

		GETACK(s, parms);
	}

	if (*gen_flags & GEN_SEND_RUN) {
		CobolToC(gout, parms->hostprocess, sizeof(gout),
			sizeof(parms->hostprocess));
		ws_syslog(LOG_DEBUG, "RUN <%s>", gout);
		if (client_send_run(s, parms, heartbeat) < 0) {
			ws_syslog(LOG_DEBUG, "RUN FAILED!");
			strncpy(parms->status, "03", 2);
			return -1;
		}

		GETACK(s, parms);
	}

	if (*gen_flags & GEN_RECV_FILE) {
		CobolToC(gout, parms->hostsendfile, sizeof(gout),
			sizeof(parms->hostsendfile));
		ws_syslog(LOG_DEBUG, "GET <%s>", gout);
		if (client_get_file(s, parms) < 0) {
			strncpy(parms->status, "17", 2);
			return -1;
		}

		GETACK(s, parms);
	}
	else if (*gen_flags & GEN_RECV_MFILE) {
		CobolToC(gout, parms->hostsendfile, sizeof(gout),
			sizeof(parms->hostsendfile));
		ws_syslog(LOG_DEBUG, "MGET <%s>", gout);
		if (client_get_multiple_files(s, parms) < 0) {
			strncpy(parms->status, "17", 2);
			return -1;
		}

		GETACK(s, parms);
	}

	if (*gen_flags & GEN_USER_PART) {
		ws_syslog(LOG_DEBUG, "DELPART");
		if (gs_send_pp_packet(s, _GEN_DELPART, NULL) < 0) {
			strncpy(parms->status, "03", 2);
			return -1;
		}

		GETACK(s, parms);
	}

	if (gs_send_done_packet(s) < 0)
		return -1;

	GETACK(s, parms);

	return 0;
}

wesco_socket_t ConnectToRemote(LPGENTRANSAREA parms)
{
	struct hostent FAR *hp;
	struct servent FAR *sp;
	struct sockaddr_in peer_address;
	char szHostName[HOSTNAMELENGTH+1];
	char szService[20];
	wesco_socket_t s;
	int offset = 0;
	int timeout;

	/*
	 *	A socket must be created and the connection made to the remote host.
	 *  The initial connection will negotiate the version to be used and then
	 *  the user will be prompted for a WESCOM user id and password.
	 */
	if (isdigit(*(parms->hostname))) {
		*szHostName = 'B';
		offset = 1;
	}

	CobolToC(szHostName+offset, parms->hostname, sizeof(szHostName)-offset,
		sizeof(parms->hostname));
	ws_syslog(LOG_INFO, "attempting to connect to %s", szHostName);
	if ((hp = ws_gethostbyname(szHostName)) == (struct hostent FAR *)NULL) {
		ws_syslog(LOG_ERR, "host %s not in /etc/hosts", szHostName);
		strncpy(parms->status, "01", 2);
		strncpy(parms->error.name, "bad host", 8);
		strncpy(parms->error.description, szHostName, strlen(szHostName));
		return SOCKET_ERROR;
	}

#if defined(_WIN32)
	GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_SERVICE_KEY,
      GENSERVER_DEFAULT_SERVICE,	szService, sizeof(szService),
      WESCOM_INI_FILE);
#else
	GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_SERVICE_KEY,
      szService, sizeof(szService), GENSERVER_DEFAULT_SERVICE,
      WESCOM_INI_FILE);
#endif

	timeout = GetPrivateProfileInt(GENSERVER_INI_SECTION,
		GENSERVER_TIMEOUT_KEY, 30, WESCOM_INI_FILE);

	/* Resolve service to entry in services file */
	if ((sp = getservbyname(szService, "tcp")) == (struct servent FAR *)NULL) {
		ws_syslog(LOG_ERR, "service %s/tcp not in /etc/services", szService);
		strncpy(parms->status, "09", 2);
		strncpy(parms->error.name, "bad servic", 10);
		strncpy(parms->error.description, szService, strlen(szService));
		return SOCKET_ERROR;
	}

	peer_address.sin_family = AF_INET;
	peer_address.sin_port = sp->s_port;
	peer_address.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		strncpy(parms->status, "03", 2);
		strncpy(parms->error.name, "socket", 6);
		strncpy(parms->error.description, "could not create", 14);
		return SOCKET_ERROR;
	}

	setsocketdebug(s, 1);
	alarm(timeout);
	if (connect(s, (struct sockaddr *)&peer_address, sizeof(struct sockaddr)) < 0) {
		alarm(0);
		ws_syslog(LOG_ERR, "connection error - %s", strerror(errno));
		strncpy(parms->status, "02", 2);
		wesco_closesocket(s);
		return SOCKET_ERROR;
	}

	alarm(0);

	ws_syslog(LOG_DEBUG, "connection successful");

	return s;
}

bool_t NegotiateVersion(wesco_socket_t s, LPGENVERSION lpGenVersion, LPGENTRANSAREA parms)
{
	bool_t bDone = FALSE;
	char szVersion[VERSIONLENGTH+1];

#if defined(_WIN32)
	strcpy(szVersion, "Current");

	do {
		if (!ReadRegReleaseInfo(lpGenVersion, szVersion))
			return FALSE;

		if (senddata(s, lpGenVersion->version, VERSIONLENGTH, 0) < VERSIONLENGTH) {
			strncpy(parms->status, "03", 2);
			strncpy(parms->error.name, "sending", 7);
			strncpy(parms->error.description, lpGenVersion->version,
				sizeof(lpGenVersion->version));
			return FALSE;
		}

		if (recvdata(s, szVersion, VERSIONLENGTH, 0) <= 0) {
			strncpy(parms->status, "03", 2);
			strncpy(parms->error.name, "receiving", 9);
			strncpy(parms->error.description, "version reply", 13);
			return FALSE;
		}

		if (!strncmp(szVersion, "OK", 2))
			bDone = TRUE;
		else {
			memset(szVersion, 0, sizeof(szVersion));
			strncpy(szVersion, lpGenVersion->version, VERSIONLENGTH);
		}
	} while (!bDone);
#else
	int nAttempt = 0;
	GENVERSION gvTable[_NUM_GS_VERSIONS_SUPPORTED] = {
		{ "\x00\x08\x00\x08\x00\x01UNX", do_genserver_commands, TRUE },
		{ "\x00\x08\x00\x05\x00\x00UNX", do_genserver_commands, FALSE },
		{ "06.05.03", GenClient060503, FALSE }
	};

	ws_syslog(LOG_DEBUG, "negotiating version");

	do {
		/* Send the data to the remote computer */
		memcpy(szVersion, gvTable[nAttempt].version, sizeof(szVersion));

		ws_syslog(LOG_DEBUG, "sending version request");

		if (senddata(s, szVersion, VERSIONLENGTH, 0) < VERSIONLENGTH) {
			CtoCobol(parms->status, "03", sizeof(parms->status));
			CtoCobol(parms->error.name, "sending", sizeof(parms->error.name));
			memcpy(parms->error.description, szVersion,	sizeof(szVersion));
			return FALSE;
		}

		ws_syslog(LOG_DEBUG, "receiving response");

		if (recvdata(s, szVersion, VERSIONLENGTH, 0) <= 0) {
			CtoCobol(parms->status, "03", sizeof(parms->status));
			CtoCobol(parms->error.name, "receiving", sizeof(parms->error.name));
			memcpy(parms->error.description, szVersion, sizeof(szVersion));
			return FALSE;
		}

		if (!strncmp(szVersion, "OK", 2)) {
			ws_syslog(LOG_DEBUG, "negotiation successful");

			bDone = TRUE;
			memcpy(lpGenVersion->version, gvTable[nAttempt].version,
				sizeof(gvTable[nAttempt].version));
			lpGenVersion->client_function = gvTable[nAttempt].client_function;
			lpGenVersion->heartbeat = gvTable[nAttempt].heartbeat;
		}
		else {
			ws_syslog(LOG_DEBUG, "negotiation impasse - try again");

			memset(szVersion, 0, sizeof(szVersion));
			nAttempt++;

			if (nAttempt >= _NUM_GS_VERSIONS_SUPPORTED) {
				ws_syslog(LOG_ERR, "version negotiation failed");
				CtoCobol(parms->status, "03", sizeof(parms->status));
				return FALSE;
			}
		}
	} while (!bDone);
#endif

	return TRUE;
}

#if defined(_WIN32)
bool_t ReadRegReleaseInfo(LPGENVERSION lpGenVersion, const char * lpVersionKey)
{
	HKEY hKey;
	int32_t result;
	int32_t dwBufferSize;
	char szKeyPath[FILENAMELENGTH+1];

	sprintf(szKeyPath, "%s\\%s", GEN_REG_KEY_RELEASE, lpVersionKey);
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKeyPath, 0, KEY_READ, &hKey);
	if (result != ERROR_SUCCESS)
		return FALSE;

	dwBufferSize = sizeof(lpGenVersion->version);
	result = RegQueryValueEx(hKey, GEN_REG_WESCOM_RELEASE, NULL, NULL,
		lpGenVersion->version, &dwBufferSize);
	if (result != ERROR_SUCCESS)
		return FALSE;

	dwBufferSize = sizeof(lpGenVersion->function);
	result = RegQueryValueEx(hKey, GEN_REG_FUNCTION_NAME, NULL, NULL, 
		lpGenVersion->function, &dwBufferSize);
	if (result != ERROR_SUCCESS)
		return FALSE;

	dwBufferSize = sizeof(lpGenVersion->dllpath);
	result = RegQueryValueEx(hKey, GEN_REG_DLL_PATH, NULL, NULL,
		lpGenVersion->dllpath, &dwBufferSize);
	if (result != ERROR_SUCCESS)
		return FALSE;

	RegCloseKey(hKey);

	return TRUE;
}
#endif
