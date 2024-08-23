#if defined(_WIN32)
#   include <windows.h>
#   include "resource.h"
#else
#   include "msdefs.h"
#endif      /* defined(_WIN32) */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define WESCOMCLIENT
#define RDIR

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "ws_syslog.h"

int gs;
wesco_handle_t ghMod;

#if defined(_WIN32)
bool_t WINAPI _CRT_INIT(HINSTANCE hDll, int32_t dwReason, wesco_void_ptr_t lpReserved);
#endif

short WINAPI WDOCGENERIC(LPGENTRANSAREA);
void DebugGTA(wesco_handle_t, LPGENTRANSAREA);
int generr;

/*
 ****************************************************************************
 *
 * Function:    WDOCGENERIC
 *
 * Description: This fuction transmits a batch file
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Return status from conversation.
 *
 ****************************************************************************
*/
short WINAPI WDOCGENERIC(LPGENTRANSAREA parameters)
{
#if defined(_WIN32)
	wesco_handle_t hFile;
	char * lpFileName = "debug.txt";
	WSADATA wsaData;
	int16_t wVersionRequested = MAKEWORD(1, 1);
#else
	struct sigaction old_alarm_action;
#endif

	char * ptr;
   int nResult;
	char szStatus[STATUSLENGTH+1];

	memset(szStatus, 0, sizeof(szStatus));

/*
	ptr = getenv("DBGGEN");
	if ((ptr != NULL) && (*ptr == 'Y'))
		ws_openlog("WDOCGENERIC", LOG_PID, LOG_LOCAL7, LOG_UPTO(LOG_DEBUG));
*/

#if defined(_WIN32)
	hFile = CreateFile(lpFileName, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return -1;

	DebugGTA(hFile, parameters);

	if (nResult = WSAStartup(wVersionRequested, &wsaData)) {
		return nResult;
	}

	if ((LOBYTE(wsaData.wVersion) != 1) || (HIBYTE(wsaData.wVersion) != 1)) {
		WSACleanup();
		return 1;
	}

	wesco_CloseHandle(hFile);
#else
	setsignal(SIGALRM, TCPAlarm, &old_alarm_action, 0, 0);
#endif

	GenClient(parameters);

	strncpy(szStatus, parameters->status, 2);

#if defined(_WIN32)
	WSACleanup();
#else
	alarm(0);
	sigaction(SIGALRM, &old_alarm_action, (struct sigaction *)NULL);
#endif

/*
	ws_syslog(LOG_DEBUG, "GTA-RET-CODE=%d", (int)(atol(szStatus)));

	if ((ptr != NULL) && (*ptr == 'Y'))
		ws_closelog();
*/

	return (int)(atol(szStatus));
}

#if defined(_WIN32)
void DebugGTA(wesco_handle_t hFile, LPGENTRANSAREA lpGtaParms)
{
	char szBuffer[4096];
	int32_t dwBytesWritten;

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_TRANS_TYPE\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->transaction,
		sizeof(lpGtaParms->transaction));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_NAME\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostname,
		sizeof(lpGtaParms->hostname));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_NAME\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientname,
		sizeof(lpGtaParms->clientname));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_OPERATION_MODE\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->opmode,
		sizeof(lpGtaParms->opmode));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);
	
	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_USER_NAME\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->username,
		sizeof(lpGtaParms->username));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);
	
	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_PARTITION_ID\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->partition,
		sizeof(lpGtaParms->partition));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);
	
	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_RET_CODE\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->status,
		sizeof(lpGtaParms->status));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);
	
	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_RET_NAME\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->error.name,
		sizeof(lpGtaParms->error.name));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_RET_DESC\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->error.description,
		sizeof(lpGtaParms->error.description));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_AREA\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostarea,
		sizeof(lpGtaParms->hostarea));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_PROCESS\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostprocess,
		sizeof(lpGtaParms->hostprocess));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_SND_FILE_NAME\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientsendfile,
		sizeof(lpGtaParms->clientsendfile));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_RCV_FILE_NAME\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostrecvfile.name,
		sizeof(lpGtaParms->hostrecvfile.name));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_RCV_FILE_PERM\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostrecvfile.permissions,
		sizeof(lpGtaParms->hostrecvfile.permissions));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_RCV_FILE_OWNER\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostrecvfile.owner,
		sizeof(lpGtaParms->hostrecvfile.owner));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_RCV_FILE_GROUP\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostrecvfile.group,
		sizeof(lpGtaParms->hostrecvfile.group));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_HOST_SND_FILE_NAME\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->hostsendfile,
		sizeof(lpGtaParms->hostsendfile));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_RCV_FILE_NAME\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientrecvfile.name,
		sizeof(lpGtaParms->clientrecvfile.name));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_RCV_FILE_PERM\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientrecvfile.permissions,
		sizeof(lpGtaParms->clientrecvfile.permissions));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_RCV_FILE_OWNER\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientrecvfile.owner,
		sizeof(lpGtaParms->clientrecvfile.owner));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_CLIENT_RCV_FILE_GROUP\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->clientrecvfile.group,
		sizeof(lpGtaParms->clientrecvfile.group));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_IO_FLAG\t\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), &(lpGtaParms->redirect),
		sizeof(lpGtaParms->redirect));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_SPECIAL_SWITCHES\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->switches,
		sizeof(lpGtaParms->switches));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);

	memset(szBuffer, 0, sizeof(szBuffer));
	wesco_wsprintf(szBuffer, "GTA_DETAIL\t\t\t\t\t\t= ");
	wesco_lstrcpyn(szBuffer+strlen(szBuffer), lpGtaParms->details,
		sizeof(lpGtaParms->details));
	wesco_lstrcat(szBuffer, "\n");
	WriteFile(hFile, (LPCVOID)szBuffer, strlen(szBuffer), &dwBytesWritten, NULL);
}

bool_t WINAPI DllMain(wesco_handle_t hDll, int32_t dwReason, wesco_void_ptr_t lpReserved)
{
	ghMod = hDll;

	if (dwReason == DLL_PROCESS_ATTACH) {
		return _CRT_INIT(hDll, dwReason, lpReserved);
	}
	else {
		return TRUE;
	}
}
#endif      /* defined(_WIN32) */
