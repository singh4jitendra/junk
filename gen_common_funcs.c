#if defined(_WIN32)
#   include <windows.h>
#   include <winbase.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#if !defined(_WIN32)
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <dirent.h>
#   include <grp.h>
#   include <pwd.h>
#   include <netdb.h>
#   include "msdefs.h"
#endif

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "gen_common_funcs.h"
#include "ws_syslog.h"

#if defined(_WIN32)
#   define RESET_UMASK(x)
#else
#   define RESET_UMASK(x) (void)umask(x)
#endif

char * cfgfile = NULL;

/*
 ******************************************************************************
 *
 * Name:        ws_recv_packet
 *
 * Description: This function receives data from a connected socket.  It 
 *              receives the size of transaction and then receives the
 *              transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to read data into.
 *              buffsize - Size of the buffer.
 *
 * Returns:     Number of bytes read in; or, -1 on error.
 *
 ******************************************************************************
*/
char * gs_set_cfg_file(char * cfg)
{
	return (cfgfile = cfg);
}

/*
 ******************************************************************************
 *
 * Name:        ws_recv_packet
 *
 * Description: This function receives data from a connected socket.  It 
 *              receives the size of transaction and then receives the
 *              transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to read data into.
 *              buffsize - Size of the buffer.
 *
 * Returns:     Number of bytes read in; or, -1 on error.
 *
 ******************************************************************************
*/
int ws_recv_packet(wesco_socket_t s, wesco_void_ptr_t lpBuffer, size_t buffer_size)
{
	int nTotal = 0;
	short message_size;

	ws_syslog(LOG_DEBUG, "ws_recv_packet: expecting %d bytes", buffer_size);
#if defined(_HEX_PACKET_INFO)
	FILE *fp = (FILE *)NULL;

	fp = fopen("/tmp/recv.hex", "a+");
#endif

	if (recvdata(s, (wesco_void_ptr_t)&message_size, sizeof(message_size), 0) < 0) {
#if defined(_HEX_PACKET_INFO)
		fprintf(fp, "ERROR: error while reading buffer\n");
		fclose(fp);
#endif
		return -1;
	}

#if defined(_HEX_PACKET_INFO)
	fprintf(fp, "packet: size %d bytes", sizeof(message_size));
	ws_hexdump(fp, (char *)&message_size, sizeof(message_size));
	fflush(fp);
#endif

	message_size = ntohs(message_size);
	ws_syslog(LOG_DEBUG, "ws_recv_packet: receiving %d bytes", message_size);
	if (buffer_size < message_size) {
#if defined(_HEX_PACKET_INFO)
		fprintf(fp, "ERROR: packet will overflow buffer\n");
		fclose(fp);
#endif
		return -1;
	}

	if ((nTotal = recvdata(s, lpBuffer, message_size, 0 )) < 0) {
		ws_syslog(LOG_DEBUG, "ws_recv_packet: %s", strerror(errno));
#if defined(_HEX_PACKET_INFO)
		fprintf(fp, "ERROR: error while reading buffer\n");
		fclose(fp);
#endif
		return -1;
	}

	ws_syslog(LOG_DEBUG, "ws_recv_packet: received %d bytes", message_size);

#if defined(_HEX_PACKET_INFO)
	fprintf(fp, "packet: size %d bytes", (size_t)message_size);
	ws_hexdump(fp, (char *)&message_size, (size_t)message_size);
	fclose(fp);
#endif

	return nTotal;
}

/*
 ******************************************************************************
 *
 * Name:        ws_send_packet
 *
 * Description: This function sends data on a connected socket.  It
 *              sends the size of transaction and then sends the
 *              transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to write data from.
 *              buffsize - Size of the buffer.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int ws_send_packet(wesco_socket_t s, wesco_void_ptr_t lpBuffer, size_t buffer_size)
{
	short message_size = htons((short)buffer_size);

	if (senddata(s, (char *)&message_size, sizeof(message_size), 0)
		< sizeof(message_size))
		return -1;

	if (senddata(s, (char *)lpBuffer, buffer_size, 0) < (int)buffer_size)
		return -1;

	return buffer_size;
}

/*
 ******************************************************************************
 *
 * Name:        ws_send_pg_packet
 *
 * Description: This function "put/get" transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              opcode   - Specifies PUT or GET.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int gs_send_pg_packet(wesco_socket_t s, short opCode, char * lpFileName,
					  mode_t mPermissions, char * lpOwner, char * lpGroup)
{
	register int nSize;
	PGPACKET pgPacket;

	/* zero out the buffer */
	memset(&pgPacket, 0, sizeof(pgPacket));

	pgPacket.opcode = htons(opCode);

	if (wesco_lstrlen(lpFileName) > FILENAMELENGTH) {
		generr = EFILENAMELENGTH;
		return -1;
	}

	wesco_lstrcpy(pgPacket.filename, lpFileName);

	pgPacket.permissions = htonl((int32_t)mPermissions);

	if (lpOwner != NULL)
		wesco_lstrcpy(pgPacket.owner, lpOwner);

	if (lpGroup != NULL)
		wesco_lstrcpy(pgPacket.group, lpGroup);

	nSize = sizeof(PGPACKET);
	if (ws_send_packet(s, &pgPacket, (short)nSize) < nSize)
		return -1;

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_put_file
 *
 * Description: This function sends a file to the remote system.
 *
 * Parameters:  s        - Socket descriptor.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int gs_put_file(wesco_socket_t s, char * lpFileName, mode_t mPerms, char * lpOwner, char * lpGroup)
{
	// Local constants
	const char *pQPrefix = "genqueue";
	const size_t sQPrefixLength = wesco_lstrlen(pQPrefix);

	// Local variables
	wesco_handle_t hFile;
	bool_t bDone = FALSE;
	NEWDATAPACKET dpPacket;
	char szTxName[FILENAMELENGTH+1];
	char szCopyFileName[FILENAMELENGTH+1];
	char szBuffer[8192];
	char * pBaseName;
	int32_t dwSeconds;
	int32_t dwMilliSeconds;
	int32_t dwBytesRead;
	int32_t dwBytesSent;
	bool_t bReadSuccess;

#if !defined(_WIN32)
	char szOwner[OWNERLENGTH+1];
	char szGroup[GROUPLENGTH+1];
	char *pTempOwner;
	size_t nTOsize;
	char *pTempGroup;
	size_t nTGsize;
	mode_t *pPerms;
	int nResult;
#endif

	strcpy(szCopyFileName, lpFileName);
	pBaseName = ws_basename(szCopyFileName);

#if defined(_WIN32)
	hFile = CreateFile(lpFileName, GENERIC_READ, 0, (LPSECURITY_ATTRIBUTES)NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, (wesco_handle_t)NULL);
#else
	hFile = open(lpFileName, O_RDONLY);
#endif
	if (hFile == INVALID_HANDLE_VALUE)
		return -1;

#if !defined(_WIN32)
	pPerms =  (mPerms > 0) ? (mode_t *)NULL : &mPerms;
		
	if (lpOwner == (char *)NULL) {
		pTempOwner = szOwner;
		nTOsize = sizeof(szOwner);
	}
	else {
		pTempOwner = (char *)NULL;
		nTOsize = (size_t)0;
	}

	if (lpGroup == (char *)NULL) {
		pTempGroup = szGroup;
		nTGsize = sizeof(szGroup);
	}
	else {
		pTempGroup = (char *)NULL;
		nTGsize = (size_t)0;
	}

	nResult = gs_get_file_info(hFile, pPerms, pTempOwner, nTOsize,
		pTempGroup, nTGsize);
	if (nResult != -1) {
		if (lpOwner == (char *)NULL)
			lpOwner = pTempOwner;

		if (lpGroup == (char *)NULL)
			lpGroup = pTempGroup;
	}
#endif
		
	if (!strncmp(lpFileName, pQPrefix, sQPrefixLength))
		sscanf(lpFileName+sQPrefixLength, "%lu.%lu.%s", &dwSeconds,
			&dwMilliSeconds, szTxName);
	else
		wesco_lstrcpy(szTxName, pBaseName);

	if (gs_send_pg_packet(s, _GEN_PUT, szTxName, mPerms, lpOwner, lpGroup) < 0) {
		wesco_CloseHandle(hFile);
		return -1;
	}

	dpPacket.opcode = htons((size_t)_GEN_DATA);

	while (!bDone) {
#if defined(_WIN32)
		bReadSuccess = ReadFile(hFile, (wesco_void_ptr_t)dpPacket.buffer,
			sizeof(dpPacket.buffer), &dwBytesRead, NULL);
#else
		dwBytesRead = read(hFile, dpPacket.buffer, sizeof(dpPacket.buffer));
		bReadSuccess = (dwBytesRead == sizeof(dpPacket.buffer));
#endif

		dpPacket.bytes = htons((short)dwBytesRead);
		if (dwBytesRead < sizeof(dpPacket.buffer)) {
			dpPacket.opcode = htons(_GEN_EOFTX);
			bDone = TRUE;
		}

		dwBytesSent = ws_send_packet(s, &dpPacket, sizeof(dpPacket));
		if (dwBytesSent < sizeof(dpPacket)) {
			wesco_CloseHandle(hFile);
			return -1;
		}
	}

	wesco_CloseHandle(hFile);

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_get_file
 *
 * Description: This function receives a file from the remote system.
 *
 * Parameters:  s        - Socket descriptor.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int gs_get_file(wesco_socket_t s, char * lpFilename, mode_t perms, char * lpOwner,
				char * lpGroup)
{
	bool_t bDone = FALSE;
	NEWDATAPACKET *dpPacket;
	PGPACKET *lpPgPacket;
	ERRORPACKET *err;
	short *pwOpCode;
	char szBuffer[sizeof(NEWDATAPACKET)];
	wesco_handle_t hFile;
	int32_t dwBytesWritten;
	int32_t dwBytesReceived;
	int32_t dwSeconds;
	int32_t dwMilliseconds;
	char szFileName[FILENAMELENGTH+1];
	char szTemp[FILENAMELENGTH+1];
	mode_t mOldMask = SET_FILE_CREATION_MASK(FCM_ALLOW_ALL);

	if (ws_recv_packet(s, szBuffer, sizeof(szBuffer)) <= 0) {
		RESET_UMASK(mOldMask);
		return -1;
	}

	pwOpCode = (short *)szBuffer;
	switch(ntohs(*pwOpCode)) {
		case _GEN_ERROR:
			err = (ERRORPACKET *)szBuffer;
			RESET_UMASK(mOldMask);
			return -1;
		default:
			lpPgPacket = (PGPACKET *)szBuffer;
			break;
	}

#if !defined(_WIN32)
	if (!perms)
		perms = ntohl(lpPgPacket->permissions);
#endif

	if ((lpFilename == NULL) || (*lpFilename <= 32))	// Is lpFilename NULL
		wesco_lstrcpy(szFileName, lpPgPacket->filename);
	else
		wesco_lstrcpy(szFileName, lpFilename);

	if (*szFileName == 0)	{								// Is lpFilename empty
		SET_FILE_CREATION_MASK(mOldMask);
		return -2;
	}

	if (strstr(szFileName, "genqueue.")) {
		sscanf(szFileName, "genqueue.%lu.%lu.%s", &dwSeconds, & dwMilliseconds,
			szTemp);
		strcpy(szFileName, szTemp);
	}

#if defined(_WIN32)
	hFile = CreateFile(szFileName, GENERIC_WRITE, 0,
		(LPSECURITY_ATTRIBUTES)NULL, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH,
		(wesco_handle_t)NULL);
#else
	hFile = creat(szFileName, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
#endif
	if (hFile == INVALID_HANDLE_VALUE) {
		SET_FILE_CREATION_MASK(mOldMask);
		return -1;
	}

	if (ntohs(*pwOpCode) != _GEN_DONE) {
		while (bDone == FALSE) {
			dwBytesReceived = ws_recv_packet(s, szBuffer, sizeof(szBuffer));
			if (dwBytesReceived <= 0) {
				wesco_CloseHandle(hFile);
				SET_FILE_CREATION_MASK(mOldMask);
				return -1;
			}

			pwOpCode = (short *)szBuffer;
			switch (ntohs(*pwOpCode)) {
				case _GEN_ERROR:
					bDone = TRUE;
					err = (ERRORPACKET *)szBuffer;
					break;
				case _GEN_EOFTX:
					bDone = TRUE;
				case _GEN_DATA:
					dpPacket = (NEWDATAPACKET *)szBuffer;
#if defined(_WIN32)
					WriteFile(hFile, dpPacket->buffer,
						(int32_t)(ntohs(dpPacket->bytes)), &dwBytesWritten,
						NULL);
#else
					dwBytesWritten = write(hFile, dpPacket->buffer,
						ntohs(dpPacket->bytes));
#endif
			}
		}
	}
	else {
		perms = S_IRWXU|S_IRWXG|S_IRWXO;
		lpOwner = (char *)NULL;
		lpGroup = (char *)NULL;
	}

#if !defined(_WIN32)
	chmod(szFileName, ((perms) ? perms : S_IRWXU|S_IRWXG|S_IRWXO));
	setgrown(szFileName, lpOwner, lpGroup);
#endif

	wesco_CloseHandle(hFile);
	SET_FILE_CREATION_MASK(mOldMask);

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_cd_packet
 *
 * Description: This function sends a "change directory" transaction.
 *
 * Parameters:  s         - Socket descriptor.
 *              directory - Name of to change to.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int gs_send_cd_packet(wesco_socket_t s, char * lpDirectory)
{
	register size_t nSize;
	CDPACKET cdPacket;

	memset(&cdPacket, 0, sizeof(cdPacket));

	cdPacket.opcode = htons(_GEN_CD);

	if (wesco_lstrlen(lpDirectory) > FILENAMELENGTH) {
		generr = EFILENAMELENGTH;
		return -1;
	}

	strcpy(cdPacket.dirname, lpDirectory);

	nSize = (size_t)sizeof(cdPacket);
	if (ws_send_packet(s, &cdPacket, nSize) < (int)nSize)
		return -1;

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        ws_chdir
 *
 * Description: This function changes the current working directory.
 *
 * Parameters:  directory - Name of to change to.
 *
 * Returns:     0 if successful; -1 on error.
 *
 ******************************************************************************
*/
int ws_chdir(char * lpPathName)
{
	return (wesco_SetCurrentDirectory(lpPathName) == CHDIR_FAILED) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_cdup
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_cdup(wesco_socket_t s)
{
	return gs_send_cd_packet(s, "..");
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_run
 *
 * Description: This function formats and transmits a "RUN" message to
 *               genserver.
 *
 * Parameters:  s          - Socket descriptor.
 *              sRedirect  - Flag to redirect io.
 *              lpGtaParms - Cobol parameters.
 *
 * Returns:     0 if successful; -1 on error.
 *
 ******************************************************************************
*/
int gs_send_run(wesco_socket_t s, short sRedirect, LPGENTRANSAREA lpGtaParms)
{
	register size_t nSize;
	RUNPACKET runPacket;

	memset(&runPacket, 0, sizeof(runPacket));

	runPacket.opcode = htons(_GEN_RUN);
	runPacket.redirect = sRedirect;

	nSize = (size_t)sizeof(runPacket);
	if (ws_send_packet(s, &runPacket, nSize) < (int)nSize) {
		generr = ESEND;
		return -1;
	}

	nSize = (size_t)sizeof(GENTRANSAREA);
	if (ws_send_packet(s, lpGtaParms, nSize) < (int)nSize) {
		generr = ESEND;
		return -1;
	}

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_get_gentransarea
 *
 * Description: This function receives a GEN-TRANS-AREA packet.  This
 *              packet contains information used by GenServer to run
 *              TRAFFICOP.
 *
 * Parameters:  s          - Socket descriptor.
 *              lpGtaParms - Pointer to buffer to store the GEN-TRANS-AREA
 *                           packet.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int gs_get_gentransarea(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
	register int nSize;
	int nBytesRead;

	nSize = sizeof(GENTRANSAREA);
	nBytesRead = ws_recv_packet(s, lpGtaParms, sizeof(GENTRANSAREA));

	return (nBytesRead < nSize) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_get_multiple_files
 *
 * Description: This function sits in a loop calling gs_get_file until
 *              all files are received.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int gs_get_multiple_files(wesco_socket_t s)
{
	bool_t bDone = FALSE;

	do {
		switch (gs_get_file(s, NULL, 0, NULL, NULL)) {
			case -2:                   /* All files received */
				bDone = TRUE;
				break;
			case -1:                   /* Error occurred */
				generr = ERECEIVE;
				return -1;
			default:
				bDone = FALSE;
		}

	} while (!bDone);

	return 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_put_multiple_files
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_put_multiple_files(wesco_socket_t s, char * lpPattern)
{
#if defined(_WIN32)
   /******************************************************************
    * This code is used to find files matching a pattern in the      *
    * Win32 environment.  It uses the FindFirstFile and FindNextFile *
    * API calls to match the patterns.                               *
    ******************************************************************/

	wesco_handle_t hFinder;
	WIN32_FIND_DATA findFileData;
   char szPath[FILENAMELENGTH+1];
   char szFileName[FILENAMELENGTH+1];
   char * lpPathPtr;

   strcpy(szPath, lpPattern);
   lpPathPtr = ws_dirname(szPath);

	// Search for files matching pattern
	hFinder = FindFirstFile(lpPattern, &findFileData);
	if (hFinder != INVALID_HANDLE_VALUE) {
		do {
         sprintf(szFileName, "%s/%s", lpPathPtr, findFileData.cFileName);
			if (gs_put_file(s, szFileName, 0, NULL, NULL) < 0) {
            gs_send_pg_packet(s, _GEN_PUT, "", 0, "", "");
				wesco_CloseHandle(hFinder);
				return -1;
			}
      } while (FindNextFile(hFinder, &findFileData));

		FindClose(hFinder);
	}
#else
   /******************************************************************
    * This code is used to find files matching a pattern in the      *
    * UNIX environment.  It uses the API calls provided by the       *
    * UNIX development environment.  The "file_match_pattern" call   *
    * is a macro that is defined using the preprocessor to be the    *
    * NCR gmatch() function or the HPUX fnmatch() function.  These   *
    * calls provide the same functionality with fnmatch() requiring  *
    * an extra parameter that is always set to 0 for our purposes.   *
    ******************************************************************/

   char szFileName[FILENAMELENGTH+1];
   char szPathName[FILENAMELENGTH+1];
	char szPattern[FILENAMELENGTH+1];
   struct stat stFileStatus;
   DIR *pDir;
   struct dirent *pEntry;

   ParseFilename(lpPattern, szPathName, szPattern);

   if ((pDir = opendir(szPathName)) == NULL)
      return -1;

   while ((pEntry = readdir(pDir)) != NULL) {
      if (file_pattern_match(pEntry->d_name, szPattern)) {
         sprintf(szFileName, "%s/%s", szPathName, pEntry->d_name);
         if (stat(szFileName, &stFileStatus) != -1) {
            if (S_ISREG(stFileStatus.st_mode)) {
               if (gs_put_file(s, szFileName, 0, NULL, NULL) < 0) {
                  gs_send_pg_packet(s, _GEN_PUT, "", 0, "", "");
						closedir(pDir);
                  return -1;
               }
            }
         }
      }
   }

	closedir(pDir);
#endif

   gs_send_pg_packet(s, _GEN_PUT, "", 0, "", "");

	return 0;
}


/*
 ******************************************************************************
 *
 * Name:        gs_send_done_packet
 *
 * Description: This function formats and transmits a "DONE" packet.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     0 if successful; -1 on error
 *
 ******************************************************************************
*/
int gs_send_done_packet(wesco_socket_t s)
{
	register size_t nSize;
	DONEPACKET donePacket;

	memset(&donePacket, 0, sizeof(donePacket));
	donePacket.opcode = htons(_GEN_DONE);

	nSize = (size_t)sizeof(donePacket);
	return (ws_send_packet(s, &donePacket, nSize) < (int)nSize) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_ack_packet
 *
 * Description: This function formats and transmits an "ACK" packet.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_ack_packet(wesco_socket_t s, short opCode)
{
	register size_t nSize;
	ACKPACKET ackPacket;

	ackPacket.opcode = htons(_GEN_ACK);
	ackPacket.acking = htons(opCode);

	nSize = (size_t)sizeof(ackPacket);
	return (ws_send_packet(s, &ackPacket, nSize) < (int)nSize) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_pp_packet
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_pp_packet(wesco_socket_t s, short opCode, char * lpFunc)
{
	register size_t nSize;
	PARTPACKET ppPacket;

	memset(&ppPacket, 0, sizeof(ppPacket));

	ppPacket.opcode = htons(opCode);

	if (lpFunc)
		wesco_lstrcpy(ppPacket.func, lpFunc);

	nSize = (size_t)sizeof(ppPacket);
	return (ws_send_packet(s, &ppPacket, nSize) <= 0) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_error_packet
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_error_packet(wesco_socket_t s, char * lpStatus, char * lpMsg)
{
	register size_t nSize;
	ERRORPACKET epPacket;

	memset(&epPacket, 0, sizeof(epPacket));

	epPacket.opcode = htons(_GEN_ERROR);
	wesco_lstrcpyn(epPacket.status, lpStatus, 2);

	if (lpMsg != NULL)
		wesco_lstrcpy(epPacket.msg, lpMsg);

	nSize = (size_t)sizeof(epPacket);
	return (ws_send_packet(s, &epPacket, nSize) < (int)nSize) ? -1 : 0;
}

/*
 ******************************************************************************
 *
 * Name:        ConnectToHost
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
wesco_socket_t ConnectToHost(LPGENTRANSAREA lpGtaParms)
{
	wesco_socket_t s;
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in peerAddress;
	char szService[25];
	char szHostName[HOSTNAMELENGTH+1];
	char *lpProtocol = "tcp";

	memset(&peerAddress, 0, sizeof(peerAddress));
	memset(szHostName, 0, sizeof(szHostName));

#if defined(_WIN32)
	GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_SERVICE_KEY,
		GENSERVER_DEFAULT_SERVICE, szService, sizeof(szService),
		WESCOM_INI_FILE);
#else
	GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_SERVICE_KEY,
		szService, sizeof(szService), GENSERVER_DEFAULT_SERVICE,
		WESCOM_INI_FILE);
#endif

	CobolToC(szHostName, lpGtaParms->hostname, sizeof(szHostName),
		sizeof(lpGtaParms->hostname));

	if ((hp = ws_gethostbyname(szHostName)) == NULL) {
		wesco_lstrcpyn(lpGtaParms->status, "01", 2);
		return INVALID_SOCKET;
	}

	if ((sp = getservbyname(szService, lpProtocol)) == NULL) {
		wesco_lstrcpyn(lpGtaParms->status, "09", 2);
		return INVALID_SOCKET;
	}

	peerAddress.sin_family = AF_INET;
	peerAddress.sin_port = sp->s_port;
	peerAddress.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		wesco_lstrcpyn(lpGtaParms->status, "03", 2);
		wesco_lstrcpyn(lpGtaParms->error.name, "socket", 6);
		wesco_lstrcpyn(lpGtaParms->error.description, "can not create", 14);
		return INVALID_SOCKET;
	}

	if (connect(s, (struct sockaddr FAR *)&peerAddress, sizeof(struct sockaddr_in)) < 0) {
		shutdown(s, 1);
		wesco_closesocket(s);
		wesco_lstrcpyn(lpGtaParms->status, "02", 2);
		return INVALID_SOCKET;
	}

	return s;
}

/*
 ******************************************************************************
 *
 * Name:        gs_send_login
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_login(wesco_socket_t s, char * lpUserName, char * lpPassword)
{
	register size_t nSize;
	PWDPACKET pwdPacket;

	memset(&pwdPacket, 0, sizeof(pwdPacket));
	pwdPacket.opcode = htons(_GEN_LOGIN);
	wesco_lstrcpy(pwdPacket.username, lpUserName);
	wesco_lstrcpy(pwdPacket.password, lpPassword);

	nSize = (size_t)sizeof(pwdPacket);
	return (ws_send_packet(s, &pwdPacket, nSize) < (int)nSize) ? -1 : 0;
}

int gs_send_heartbeat(wesco_socket_t s)
{
	register size_t nSize;
	HRTPACKET hrtPacket;

	memset(&hrtPacket, 0, sizeof(hrtPacket));
	hrtPacket.opcode = htons(_GEN_HRTBEAT);

	nSize = (size_t)sizeof(hrtPacket);
	return (ws_send_packet(s, &hrtPacket, nSize) < (int)nSize) ? -1 : 0;
}

int gs_inspect_message(wesco_socket_t s)
{
	int rv;
	int bytes;
	short msgsize;
	char buffer[256];
	short opcode;

	bytes = recv(s, (char *)&msgsize, sizeof(msgsize), MSG_PEEK);
	if (bytes < sizeof(msgsize))
		return -1;

	msgsize = ntohs(msgsize);
	if (msgsize == sizeof(GENTRANSAREA)) {
		rv = -1;
	}
	else {
		if (ws_recv_packet(s, buffer, sizeof(buffer)) < 1) {
			rv = -1;
		}
		else {
			opcode = ntohs((short)buffer);
			switch (opcode) {
				case _GEN_HRTBEAT:
					gs_send_ack_packet(s, opcode);
					bytes = recv(s, (char *)&msgsize, sizeof(msgsize), MSG_PEEK);
					rv = 0;
					break;
				case _GEN_ACK:
					/* Yeah, this looks a little complicated; but, it's actually
						quite simple.  I'm getting the "acking" field from the
						ACKPACKET structure. */
					if (ntohs((short)(*(buffer + sizeof(short)))) == _GEN_HRTBEAT) {
						bytes = recv(s, (char *)&msgsize, sizeof(msgsize), MSG_PEEK);
						rv = 0;
					}
					else {
						rv = -1;
					}

					break;
				default:
					rv = -1;
			}
		}
	}

	return rv;
}

/*
 ******************************************************************************
 *
 * Name:        ws_create_remote_connection
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
wesco_socket_t ws_create_remote_connection(struct sockaddr_in *pa)
{
   wesco_socket_t io_s;

   if ((io_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return INVALID_SOCKET;

#if !defined(_WIN32)
   alarm(60);
#endif

   if (connect(io_s, (const struct sockaddr *)pa, sizeof(struct sockaddr_in)) < 0) {
      wesco_closesocket(io_s);

#if !defined(_WIN32)
	   alarm(0);
#endif

      return INVALID_SOCKET;
   }

#if !defined(_WIN32)
   alarm(0);
#endif

   return io_s;
}

/*
 ******************************************************************************
 *
 * Name:        ws_create_remote_connection
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int gs_send_evar(wesco_socket_t s, char * lpVarName)
{
	char * lpValue;
	char szBuffer[_POSIX_ARG_MAX];

	if (*lpVarName == 0)
		return ws_send_packet(s, (wesco_void_ptr_t)lpVarName, (short)1);
	else {
		if ((lpValue = getenv(lpVarName)) == (char *)NULL)
			return -1;
		else {
			sprintf(szBuffer, "%s=%s", lpVarName, lpValue);
			return ws_send_packet(s, (wesco_void_ptr_t)szBuffer,
				(short)strlen(szBuffer)+1);
		}
	}
}

int gs_recv_evar(wesco_socket_t s)
{
	char szBuffer[_POSIX_ARG_MAX];
    //Initializing zdBuffer to be null values.
    memset(szBuffer, 0, sizeof(szBuffer));
	char *lpMem;
	int nBytes;

	if ((nBytes = ws_recv_packet(s, szBuffer, sizeof(szBuffer))) <= 0) {
		return -1;
	}
	else if (*szBuffer == 0) {
		return 1;
	}
	else {
		nBytes++; /* for safety's sake, allocate an extra byte to add a null */
		if (strcmp(szBuffer, GS_UNKNOWN_TERM) == 0) {
			if ((lpMem = calloc(sizeof(char), sizeof(GS_DEFAULT_TERM))) == NULL) {
				return -1;
			}

			strcpy(lpMem, GS_DEFAULT_TERM);
		}
		else {
			if ((lpMem = calloc(sizeof(char), nBytes)) == (char *)NULL) {
				return -1;
			}
            //Changed strcpy to strncpy.  This is done to prevent junk data from being copied into lpMem.
			strncpy(lpMem, szBuffer, nBytes - 1);
		}

		if (putenv(lpMem) != 0)
			return -1;

		return 0;
	}
}

int ws_create_PG_from_REMOTEFILE(PGPACKET *lpPG, REMOTEFILE *lpRF)
{
   char szTemp[PERMISSIONSLENGTH+1];

   memset(lpPG, 0, sizeof(PGPACKET));

   CobolToC(lpPG->filename, lpRF->name, sizeof(lpPG->filename), sizeof(lpRF->name));
   CobolToC(szTemp, lpRF->permissions, sizeof(szTemp), sizeof(lpRF->permissions));
   CobolToC(lpPG->owner, lpRF->owner, sizeof(lpPG->owner), sizeof(lpRF->owner));
   CobolToC(lpPG->group, lpRF->group, sizeof(lpPG->group), sizeof(lpRF->group));

   sscanf(szTemp, "%o", &(lpPG->permissions));

   return 0;
}

#if !defined(_WIN32)
int gs_get_file_info(int fd, mode_t *pMode, char *lpOwner, size_t nOwnerSize, char *lpGroup, size_t nGroupSize)
{
	struct group *pGroupEntry;
	struct passwd *pPasswdEntry;
	struct stat fsStatus;
	size_t nLength;

	if (fstat(fd, &fsStatus) == -1)
		return -1;

	if (pMode != (mode_t *)NULL)
		*pMode = fsStatus.st_mode;

	if ((lpOwner != (char *)NULL) && (nOwnerSize > 1)) {
		pPasswdEntry = getpwuid(fsStatus.st_uid);
		if (pPasswdEntry == (struct passwd *)NULL)
			sprintf(lpOwner, "%d", fsStatus.st_uid);
		else {
			nLength = wesco_lstrlen(pPasswdEntry->pw_name);
			if (nLength >= nOwnerSize) {
				wesco_lstrcpyn(lpOwner, pPasswdEntry->pw_name, nOwnerSize - 1);
				*(lpOwner + (nOwnerSize - 1)) = 0;
			}
			else
				strcpy(lpOwner, pPasswdEntry->pw_name);
		}
	}

	if ((lpGroup != (char *)NULL) && (nGroupSize > 1)) {
		pGroupEntry = getgrgid(fsStatus.st_gid);
		if (pGroupEntry == (struct group *)NULL)
			sprintf(lpGroup, "%d", fsStatus.st_gid);
		else {
			nLength = wesco_lstrlen(pGroupEntry->gr_name);
			if (nLength >= nGroupSize) {
				wesco_lstrcpyn(lpGroup, pGroupEntry->gr_name, nGroupSize - 1);
				*(lpGroup + (nGroupSize - 1)) = 0;
			}
			else
				strcpy(lpGroup, pGroupEntry->gr_name);
		}
	}
}
#endif
