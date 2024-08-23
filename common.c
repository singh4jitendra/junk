#if defined(_WIN32)
#   include <windows.h>
#else
#   include "msdefs.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#if defined(_WIN32)
#   include <io.h>
#   include <direct.h>
#else
#   include <sys/stat.h>
#   include <netinet/in.h>
#   include <libgen.h>
#   include <dirent.h>
#   include <netdb.h>
#   include <pwd.h>
#   include <grp.h>
#endif

#include "common.h"

#ifndef NETWORKLOG
#define NETWORKLOG (-1)
#endif

/*
****************************************************************************
*
*  Function:    ISONNET
*
*  Description: This function determines if a branch is on the network.
* 
*  Parameters:  parameters - Includes branch and status ares
*                                              
*  Returns:     Nothing
*
****************************************************************************
*/
void ISONNET(LPFUNCPARMS lpFuncParms)
{
   char szHost[HOSTNAMELENGTH+2];
   struct hostent *hp;
   char **cur_alias;

   memset(szHost, 0, sizeof(szHost));

   *szHost = 'B';
   CobolToC(szHost + 1, lpFuncParms->hostname, sizeof(szHost) - 1,
      sizeof(lpFuncParms->hostname));

   hp = gethostbyname(szHost);
   if (hp == NULL) {
      strncpy(lpFuncParms->status, "12", 2);
      return;
   }

	/* check "official" name first */
	if (!wesco_lstrcmp(szHost+1, hp->h_name)) {
		strncpy(lpFuncParms->status, "00", 2);
		return;
	}

	/* check the aliases next */
   cur_alias = hp->h_aliases;
   for (; *cur_alias != NULL && wesco_lstrcmp(szHost+1, *cur_alias); cur_alias++);

   if (*cur_alias == NULL)
      strncpy(lpFuncParms->status, "12", 2);
   else
      strncpy(lpFuncParms->status, "00", 2);
}

/*
****************************************************************************
*
*  Function:    TransmitFile
*
*  Description: This function transmits the specified file
* 
*  Parameters:  filename - The name of file to transmit     
*               s        - Handle to the socket
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
int WescoTransmitFile(wesco_socket_t s, char * lpFileName)
{
   char szBuffer[WESCO_FILE_PACKET_TOTAL_SIZE];
   char szNumBytes[WESCO_FILE_PACKET_INFO_SIZE+1];
   bool_t bDone = FALSE;
   bool_t bReadSuccess;
   int32_t dwBytesRead;
   wesco_handle_t hFile;

#if defined(_WIN32)
   hFile = CreateFile(lpFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
      FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#else
   hFile = open(lpFileName, O_RDONLY);
#endif
   if (hFile == INVALID_HANDLE_VALUE)
      return FALSE;

   while (!bDone) {
      memset(szBuffer, 32, sizeof(szBuffer));

#if defined(_WIN32)
      bReadSuccess = ReadFile(hFile, (szBuffer+WESCO_FILE_PACKET_INFO_SIZE),
         WESCO_FILE_PACKET_DATA_SIZE, &dwBytesRead, NULL);
#else
      dwBytesRead = read(hFile, szBuffer+WESCO_FILE_PACKET_INFO_SIZE,
         WESCO_FILE_PACKET_DATA_SIZE);
      bReadSuccess = (dwBytesRead == WESCO_FILE_PACKET_DATA_SIZE);
#endif

      if (!bReadSuccess) {
         wesco_CloseHandle(hFile);
         return FALSE;
      }

      if (dwBytesRead < WESCO_FILE_PACKET_DATA_SIZE)
         bDone = TRUE;

      sprintf(szNumBytes, WESCO_FILE_PACKET_PRINTF_FMT, dwBytesRead);
      strncpy(szBuffer, szNumBytes, WESCO_FILE_PACKET_INFO_SIZE);

      if (send(s, szBuffer, WESCO_FILE_PACKET_TOTAL_SIZE, 0) != WESCO_FILE_PACKET_TOTAL_SIZE) {
         wesco_CloseHandle(hFile);
         return FALSE;
      }
   }

   wesco_CloseHandle(hFile);

   return TRUE;
}

/*
****************************************************************************
*
*  Function:    WescoReceiveFile
*
*  Description: This function receives the specified file
* 
*  Parameters:  filename - The name of file to transmit     
*               s        - Handle to the socket
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
bool_t WescoReceiveFile(wesco_socket_t s, char * lpFileName)
{
   char szBuffer[WESCO_FILE_PACKET_TOTAL_SIZE+1];
   char szTemp[WESCO_FILE_PACKET_INFO_SIZE+1];
   bool_t bDone = FALSE;
   int32_t dwPacketDataSize;
   int32_t dwBytesRead;
   int32_t dwBytesWritten;
   wesco_handle_t hFile;

#if defined(_WIN32)
   hFile = CreateFile(lpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
      FILE_FLAG_WRITE_THROUGH, NULL);
#else
   hFile = creat(lpFileName, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
#endif
   if (hFile == INVALID_HANDLE_VALUE)
      return FALSE;

   /*
      This loop will be executed until the file is finished being
      transmitted.  The first four bytes in the buffer indicate the
      number of bytes that are being transmitted in the current
      buffer.  If the number of bytes transmitted is less than 1020
      then this is the last segment of the transmission.
   */
   do {
      memset(szBuffer, 32, sizeof(szBuffer));
      memset(szTemp, 0, sizeof(szTemp));
      szBuffer[sizeof(szBuffer)-1] = 0;

      dwBytesRead = recvdata(s, szBuffer, sizeof(szBuffer)-1, 0);
      if (dwBytesRead < 1) {
         wesco_CloseHandle(hFile);
         return FALSE;
      }

      strncpy(szTemp, szBuffer, sizeof(szTemp));
      szTemp[sizeof(szTemp)-1] = (char)0;				/* The last byte should be a null (0) */
      sscanf(szTemp, WESCO_FILE_PACKET_SCANF_FMT, &dwPacketDataSize);

      if (dwPacketDataSize < WESCO_FILE_PACKET_DATA_SIZE)
         bDone = TRUE;

#if defined(_WIN32)
      if (dwPacketDataSize > 0)
         WriteFile(hFile, (LPCVOID)(szBuffer+WESCO_FILE_PACKET_INFO_SIZE),
            dwPacketDataSize, &dwBytesWritten, NULL);
#else
      if (dwPacketDataSize > 0)
         dwBytesWritten = write(hFile, szBuffer+WESCO_FILE_PACKET_INFO_SIZE,
            dwPacketDataSize);
#endif
   } while (!bDone);

   wesco_CloseHandle(hFile);

   return TRUE;
}

/*
****************************************************************************
*
*  Function:    ParseFilename
*
*  Description: This function parses a full filename into path and name
* 
*  Parameters:  fullname - The full name and path to the file
*               path     - The path to the file
*               filename - The name of the file
*                                              
*  Returns:     TRUE if parsed, else FALSE
*
****************************************************************************
*/
bool_t ParseFilename(const char * lpFullName, char * lpPath, char * lpFileName)
{
   char szTempFileName[FILENAMELENGTH+1];
   char szTempPath[FILENAMELENGTH+1];

   wesco_lstrcpy(szTempFileName, lpFullName);
   wesco_lstrcpy(szTempPath, lpFullName);

   wesco_lstrcpy(lpPath, ws_dirname(szTempPath));
   wesco_lstrcpy(lpFileName, ws_basename(szTempFileName));

   return TRUE;
}

/*
****************************************************************************
*
*  Function:    CtoCobol
*
*  Description: This function copies a C string into a COBOL string
* 
*  Parameters:  cobolStr  - Pointer to the COBOL string
*               cStr      - Pointer to the C string
*               cobolSize - Number of bytes to copy
*                                              
*  Returns:     TRUE if parsed, else FALSE
*
****************************************************************************
*/
int CtoCobol(char * cobolStr, const char * cStr, int cobolSize)
{
   int cStrLen, retVal = GOODCONVERSION, numCopyBytes;

   cStrLen = wesco_lstrlen(cStr);

   if (cStrLen <= cobolSize)
      numCopyBytes = cStrLen;
   else {
      numCopyBytes = cobolSize;
      retVal = STRING_TOO_LARGE;
   }

   memset(cobolStr, 32, cobolSize);
   strncpy(cobolStr, cStr, numCopyBytes);

   return retVal;			
}

/*
****************************************************************************
*
*  Function:    CobolToC
*
*  Description: This function copies a COBOL string into a C string
* 
*  Parameters:  cStr      - Pointer to the C string
*               cobolStr  - Pointer to the COBOL string
*               cSize     - Max size of the C string
*               cobolSize - Size of the COBOL string
*                                              
*  Returns:     TRUE if parsed, else FALSE
*
****************************************************************************
*/
int CobolToC(char * cStr, const char * cobolStr, int cSize, int cobolSize)
{
   int pos, copyNumBytes, retVal = GOODCONVERSION;

   /*
      Loop through the cobol string until either a space is encountered
      or the entire string was checked.  The position in the string is then
      compared to the space allocated for the C string.  If the cobol 
      string is too large only the first cSize - 1 bytes are copied and
      the STRING_TOO_LARGE value is returned.  The minus one allows room 
      for the null character in the C string.
   */
   for (pos = cobolSize-1; cobolStr[pos] == ' ' && pos >= 0; pos--);

   if (pos > cSize) {
      copyNumBytes = cSize - 1;
      retVal = STRING_TOO_LARGE;
   }
   else
      copyNumBytes = pos+1;

   strncpy(cStr, cobolStr, copyNumBytes);
   cStr[copyNumBytes] = 0;

   return retVal;
}

/*
****************************************************************************
*
*  Function:    FileSend
*
*  Description: This function transmits a file to a remote computer
* 
*  Parameters:  s        - Socket descriptor 
*               fileInfo - Structure containing file information
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
int FileSend(wesco_socket_t s, REMOTEFILE rfFileInfo)
{
   char szFileName[FILENAMELENGTH+1];

   CobolToC(szFileName, rfFileInfo.name, FILENAMELENGTH+1, FILENAMELENGTH);

   return WescoTransmitFile(s, szFileName);
}

/*
****************************************************************************
*
*  Function:    FileReceive
*
*  Description: This function receives a file from a remote computer
* 
*  Parameters:  s        - Socket descriptor 
*               fileInfo - Structure containing file information
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
int FileReceive(wesco_socket_t s, REMOTEFILE rfFileInfo)
{
   char szFileName[FILENAMELENGTH+1];
   char szOwner[OWNERLENGTH+1]; 
   char szGroup[GROUPLENGTH+1];
   char szPermStr[PERMISSIONSLENGTH+1];
   char szPath[FILENAMELENGTH+1];
   char szFullPath[FILENAMELENGTH+1];
   char szFullName[FILENAMELENGTH+1];
   int32_t permissions, count;

   CobolToC(szFullName, rfFileInfo.name, sizeof(szFullName),
      sizeof(rfFileInfo.name));
   ParseFilename(szFullName, szPath, szFileName);
   if (szPath[0] != 0)
      CheckDirectory(szPath);

   CobolToC(szFullPath, rfFileInfo.name, FILENAMELENGTH+1, FILENAMELENGTH);
   CobolToC(szOwner, rfFileInfo.owner, OWNERLENGTH+1, OWNERLENGTH);
   CobolToC(szGroup, rfFileInfo.group, GROUPLENGTH+1, GROUPLENGTH);
   CobolToC(szPermStr, rfFileInfo.permissions, PERMISSIONSLENGTH+1,
      PERMISSIONSLENGTH);

   for (count = 0; count < 4; count++) {
      if (szPermStr[count] == ' ')
         szPermStr[count] = '0';
   }

   if (!sscanf(szPermStr, "%0004o", &permissions))
      permissions = 0;

   return WescoReceiveFile(s, szFullPath);
}

/*
****************************************************************************
*
*  Function:    FileMultiSend
*
*  Description: This function transmits multiple files to a remote computer
* 
*  Parameters:  s        - Socket descriptor 
*               fileInfo - Structure containing file information
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
int FileMultiSend(wesco_socket_t s, REMOTEFILE rfFileInfo)
{
   char szPattern[FILENAMELENGTH+1];
   REMOTEFILE rfSingleFile, rfSendFile;

#if defined(_WIN32)
   wesco_handle_t hFinder;
   WIN32_FIND_DATA fdFileData;
#else
   DIR *pDirectory;
   struct dirent *pDirEntry;
   struct passwd *pPasswordEntry;
   struct group *pGroupEntry;
   struct stat stFileStatus;
   char szPermissions[10];
   char szPath[FILENAMELENGTH+1];
   char szFullPath[FILENAMELENGTH+1];
   char szFileName[FILENAMELENGTH+1];
#endif

#if defined(_WIN32)
   CobolToC(szPattern, rfFileInfo.name, FILENAMELENGTH+1, FILENAMELENGTH);

   if ((hFinder = FindFirstFile(szPattern, &fdFileData)) != INVALID_HANDLE_VALUE) {
      while (FindNextFile(hFinder, &fdFileData)) {
         memset(&rfSingleFile, 0, sizeof(rfSingleFile));
         memset(&rfSendFile, 32, sizeof(rfSendFile));
         CtoCobol(rfSendFile.name, fdFileData.cFileName,
            wesco_lstrlen(fdFileData.cFileName));

         senddata(s, (char *)&rfSendFile, sizeof(rfSendFile), 0);
         FileSend(s, rfSingleFile);
      }
   }
#else
   CobolToC(szFullPath, rfFileInfo.name, sizeof(szFullPath),
      sizeof(rfFileInfo.name));
   ParseFilename(szFullPath, szPath, szFileName);

   if (szPath[wesco_lstrlen(szPath)-1] == '/')
      szPath[wesco_lstrlen(szPath)-1] = 0;

   if ((pDirectory = opendir(szPath)) == (DIR *)NULL)
      return FALSE;

   while ((pDirEntry = readdir(pDirectory)) != (struct dirent *)NULL) {
      if (file_pattern_match(pDirEntry->d_name, szFileName)) {
         if (stat(pDirEntry->d_name, &stFileStatus) != -1) {
            memset(&rfSingleFile, 0, sizeof(rfSingleFile));
            memset(&rfSendFile, 32, sizeof(rfSendFile));

            wesco_lstrcpy(rfSingleFile.name, szPath);
            wesco_lstrcat(rfSingleFile.name, "/");
            wesco_lstrcat(rfSingleFile.name, pDirEntry->d_name);
            CtoCobol(rfSendFile.name, rfSingleFile.name,
               wesco_lstrlen(rfSingleFile.name));

            sprintf(szPermissions, "%04o", stFileStatus.st_mode);
            CtoCobol(rfSendFile.permissions, szPermissions,
               wesco_lstrlen(szPermissions));

            if ((pPasswordEntry = getpwuid(stFileStatus.st_uid)) != NULL)
               CtoCobol(rfSendFile.owner, pPasswordEntry->pw_name,
                  wesco_lstrlen(pPasswordEntry->pw_name));

            if ((pGroupEntry = getgrgid(stFileStatus.st_gid)) != NULL)
               CtoCobol(rfSendFile.group, pGroupEntry->gr_name,
                  wesco_lstrlen(pGroupEntry->gr_name));
         }
      }
   }

	closedir(pDirectory);
#endif

   memset(&rfSingleFile, 32, sizeof(rfSingleFile));
   senddata(s, (char*)&rfSingleFile, sizeof(rfSingleFile), 0);

   return TRUE;
}

/*
****************************************************************************
*
*  Function:    FileMultiReceive
*
*  Description: This function receives multiple files from a remote computer
* 
*  Parameters:  s        - Socket descriptor 
*               fileInfo - Structure containing file information
*                                              
*  Returns:     TRUE if transmitted, else FALSE
*
****************************************************************************
*/
int FileMultiReceive(wesco_socket_t s, REMOTEFILE rfFileInfo)
{
   bool_t bDone = FALSE;
   char szBuffer[FILENAMELENGTH+1];
   char szPath[FILENAMELENGTH+1];
   char szFullPath[FILENAMELENGTH+1];
   char szFileName[FILENAMELENGTH+1];
   REMOTEFILE rfSingleFile;

   /*
      The function will loop until a filename buffer is received with
      a space in the first postition.  This indicates that all files
      have been transmitted.
   */
   while (!bDone) {
      memset(szFullPath, 0, sizeof(szFullPath));
      memset(&rfSingleFile, 0, sizeof(rfSingleFile));
      recvdata(s, (char*)&rfSingleFile, sizeof(rfSingleFile), 0);
      wesco_lstrcpy(szFullPath, rfSingleFile.name);

      if (*szFullPath == ' ')
         bDone = TRUE;
      else {
         /*
            Generate the REMOTEFILE structure needed to receive a file.
            This structure provides information on the permissions,
            name, owner, and group.  If the filename includes a 
            sub-directory, test to see if the sub-directory exists.
            If not, create the sub-directory.
         */                 
         ParseFilename(szFullPath, szPath, szFileName);

         CobolToC(szBuffer, rfFileInfo.name, sizeof(szBuffer),
            sizeof(rfFileInfo.name));

         memset(rfSingleFile.name, 0, sizeof(rfSingleFile.name));
         wesco_lstrcat(szBuffer, szFileName);

         CtoCobol(rfSingleFile.name, szBuffer, sizeof(rfSingleFile.name));
         if (rfFileInfo.permissions[0] != ' ')
            strncpy(rfSingleFile.permissions, rfFileInfo.permissions,
               sizeof(rfFileInfo.permissions));
         
         if (rfFileInfo.owner[0] != ' ')
            strncpy(rfSingleFile.owner, rfFileInfo.owner,
               sizeof(rfSingleFile.owner));

         if (rfFileInfo.group[0] != ' ')
            strncpy(rfSingleFile.group, rfFileInfo.group,
               sizeof(rfSingleFile.group));

         FileReceive(s, rfSingleFile);
      }
   }

   return TRUE;
}

/*
****************************************************************************
*
*  Function:    CheckDirectory
*
*  Description: This function checks to see if the specified directory
*               exists.  If not, then the directory is created.
* 
*  Parameters:  dirName  - The name of the directory
*               fileInfo - Structure containing file information
*                                              
*  Returns:     status
*
****************************************************************************
*/
int CheckDirectory(const char * lpDirectoryName)
{
   int nReturnCode = DIRECTORY_EXISTS;
   char szPathName[FILENAMELENGTH+1];
   char szFileName[FILENAMELENGTH+1];
   char * lpPath;
   char * lpFile;

   if (!wesco_lstrcmp(lpDirectoryName, ".") || !wesco_lstrcmp(lpDirectoryName, ".."))
      return DIRECTORY_EXISTS;

   wesco_lstrcpy(szPathName, lpDirectoryName);
   wesco_lstrcpy(szFileName, lpDirectoryName);

   lpPath = ws_dirname(szPathName);
   lpFile = ws_basename(szFileName);

   if (!wesco_lstrcmp(lpPath, "/"))
      return DIRECTORY_EXISTS;

   if (CheckDirectory(lpPath) == DIRECTORY_ERROR)
      return DIRECTORY_ERROR;

#if defined(_WIN32)
   if (!CreateDirectory(lpDirectoryName, NULL)) {
#else
   if (mkdir(lpDirectoryName, ALLPERM)) {
#endif
      if (errno == EEXIST)
         return DIRECTORY_EXISTS;
      else
         return DIRECTORY_ERROR;
   }
   else
      return DIRECTORY_CREATED;

   return nReturnCode;
}

/*
****************************************************************************
*
*  Function:    NetworkError
*
*  Description: This function logs a network\server error to the error
*               log.  If the error is considered a critical error the
*               log file is immediately sent to WESDOC.
* 
*  Parameters:  message  - Pointer to the error message
*               critical - Flag indicating if the error is a critical error
*                                              
*  Returns:     Nothing
*
****************************************************************************
*/
void NetworkError(const char * message, int critical)
{
	int fd;
	FILE *fp;
	time_t current_time;
	struct tm * ptmTime;
	char szTimeStamp[50];
	struct flock file_lock;
	char *networkErrorLogName = WS_NETWORK_ERROR_LOG;

	file_lock.l_type = F_WRLCK;
	file_lock.l_whence = SEEK_SET;
	file_lock.l_start = (off_t)0;
	file_lock.l_len = (off_t)0;
	file_lock.l_pid = getpid();

	time(&current_time);
	ptmTime = localtime(&current_time);
	strftime(szTimeStamp, sizeof(szTimeStamp), "%m/%d/%Y  %H:%M:%S",
		ptmTime);

   /*
      Open the log file and write the error message to the file.  If
      it is a critical error send the log to WESDOC immediately.
   */
   if ((fp = fopen(networkErrorLogName, "at")) != (FILE *)NULL) {
		fd = fileno(fp);    /* get file descriptor for stream */

		/* set a write lock on the file */
		if (fcntl(fd, F_SETLKW, &file_lock) < 0) {
			fclose(fp);
			return;
		}

		fprintf(fp, "%s : %d: %s\n", szTimeStamp, getpid(), message);

		/* release the lock and close the file */
		file_lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, file_lock);
		fclose(fp);

		if (critical)
			SendLogImmediately();
	}
}

/*
****************************************************************************
*
*  Function:    NetworkError
*
*  Description: This function sends the network error log to WESDOC before
*               the next scheduled transfer because of a critical error.
* 
*  Parameters:  None
*                                              
*  Returns:     Nothing
*
****************************************************************************
*/
void SendLogImmediately(void)
{
#if 0
   char logName[FILENAMELENGTH+1];
   COBOLPARMS parms;

   memset(parms, 32, sizeof(COBOLPARMS));

   /* The "7845" will be replaced with the actual branch name. */
   wesco_lstrcpy(logName, "7845");
   wesco_lstrcat(logName, ".network.error.log"

   strncpy(parms.opmode, "02", OPMODELENGTH);
   strncpy(parms.clientsendfile, networkErrorLogName,
   wesco_lstrlen(networkErrorLogName));
   strnpyn(parms.hostrecvfile.name, logName, strlen(logName));
#endif
}

void TCPAlarm(int nSigNo)
{
   nSigNo ^= nSigNo;
}

int AuthorizeConnection(wesco_socket_t s, struct hostent **hp, struct sockaddr_in *peer)
{
   *hp = gethostbyaddr((char*)&peer->sin_addr, sizeof(struct in_addr),
      peer->sin_family);

   return !(*hp == NULL);
}

int WescoGetVersion(wesco_socket_t s, char * version, const char * progName, const char * hostName)
{
   int nBytesReceived;

   nBytesReceived = recvdata(s, version, VERSIONLENGTH, 0);
   if (nBytesReceived < 1) {
#if 0
      wescolog(NETWORKLOG, "%s: connection with %s terminated unexpectedly",
      progName, hostName);
#endif
      return FALSE;
   }

   return TRUE;
}

char * ws_basename(char * lpPathName)
{
   char * lpLastForwardSlash;
   char * lpLastBackSlash;
   char * lpPtr;

   if ((lpPathName == (char *)NULL) || (*lpPathName == 0))
      return ".";

   /* Ignore the drive specification for now. */
   if ((lpPtr = strchr(lpPathName, ':')) != NULL)
      lpPtr++;
   else
      lpPtr = lpPathName;

   if ((!wesco_lstrcmp(lpPtr, "/")) || (!wesco_lstrcmp(lpPtr, "\\")))
      return lpPtr;

   /* Check for forward slashes and backslashes.
      Find the last slash in the path. */
   lpLastForwardSlash = strrchr(lpPtr, '/');
   lpLastBackSlash = strrchr(lpPtr, '\\');

   if ((lpLastForwardSlash == (char *)NULL) && 
      (lpLastBackSlash == (char *)NULL))
      return lpPtr;

   lpPtr = (lpLastForwardSlash > lpLastBackSlash) ? lpLastForwardSlash : 
      lpLastBackSlash;

   /* If the slash is the last character,
   replace with a null character and try again. */
   if (*(lpPtr + 1) == 0) {
      *lpPtr = 0;
      return ws_basename(lpPathName);
   }

   return lpPtr + 1;
}

char * ws_dirname(char * lpPathName)
{
   char * lpLastForwardSlash;
   char * lpLastBackSlash;
   char * lpDir;
   char * lpPtr;

   if ((!strcmp(lpPathName, ".")) || (!strcmp(lpPathName, "..")) ||
      (lpPathName == (char *)NULL) || (*lpPathName == 0))
      return (char *)".";

   /* Ignore the drive specification for now. */
   if ((lpPtr = strchr(lpPathName, ':')) != NULL)
      lpPtr++;
   else
      lpPtr = lpPathName;

   if ((!strcmp(lpPtr, "/")) || (!strcmp(lpPtr, "\\")))
      return lpPathName;

   /* Check for forward slashes and backslashes.
      Find the last slash in the path. */
   lpLastForwardSlash = strrchr(lpPtr, '/');
   lpLastBackSlash = strrchr(lpPtr, '\\');

   if ((lpLastForwardSlash == (char *)NULL) && (lpLastBackSlash == (char *)NULL))
      return ".";

   lpDir = (lpLastForwardSlash > lpLastBackSlash) ? lpLastForwardSlash :
      lpLastBackSlash;
   if (lpDir == lpPtr)
      return "/";

   /* If the slash is the last character,
      replace with a null character and try again. */
   if (*(lpDir + 1) == 0) {
      *lpDir = 0;
      return ws_dirname(lpPathName);
   }

   *lpDir = 0;

   return lpPathName;
}

struct hostent *ws_gethostbyname(char * lpHostName)
{
	char szHost[256];
	struct hostent *hp;
	char **cur_alias;

	memset(szHost, 0, sizeof(szHost));

	if (isdigit(*lpHostName))
		sprintf(szHost, "B%s", lpHostName);
	else
		wesco_lstrcpy(szHost, (const char *)lpHostName);

	if ((hp = gethostbyname(szHost)) == (struct hostent *)NULL)
		return (struct hostent *)NULL;

	/* check "official" name first */
	if (!wesco_lstrcmp(lpHostName, hp->h_name)) {
		return hp;
	}

	cur_alias = hp->h_aliases;
	for (; *cur_alias != NULL && wesco_lstrcmp(lpHostName, *cur_alias); cur_alias++);

	return (*cur_alias == NULL) ? (struct hostent *)NULL : hp;
}

char * get_cfg_file(char * buffer, size_t bufsize)
{
	char * ptr;

	ptr = getenv(WS_CFG);
	if (ptr == NULL) {
		/* variable not set, use default value */
		ptr = WS_DEFAULT_CFG_FILE;
	}

	if (ptr != NULL) {
		if (buffer != NULL) {
			/* make copy of string if buffer is provided */
			if (strlen(ptr) < bufsize)
				strncpy(buffer, ptr, bufsize);
			else
				memset(buffer, 0, bufsize);
		}
	}

	return ptr;
}

#if !defined(_WIN32)
/*
* Functions in this section are provided for backwards compatability
* with earlier versions of the shared library.  They are only included
* in the NCR release of the library.
*/

short endianswap(short original)
{
#if defined(_HPUX_SOURCE)
   union {
      short number;
      char byte[2];
   } forward, backward;

   forward.number = original;
   backward.byte[0] = forward.byte[1];
   backward.byte[1] = forward.byte[0];

   return backward.number;
#else
   return original;
#endif
}

int32_t lendianswap(int32_t original)
{
#if defined(_HPUX_SOURCE)
   union {
      int32_t number;
      char byte[4];
   } forward, backward;

   forward.number = original;
   backward.byte[2] = forward.byte[1];
   backward.byte[3] = forward.byte[0];
   backward.byte[0] = forward.byte[3];
   backward.byte[1] = forward.byte[2];

   return backward.number;
#else
   return original;
#endif
}

int SendBuffer(char *caller, int s, char *buffer, int size)
{
   if (send(s, buffer, size, 0) != size) {
      if (caller != NULL) {
         perror(caller);
         fprintf(stderr, "%s: send error...connection terminated\n",
            caller);
      }

      return FALSE;
   }

   return TRUE;
}

int TransmitFile(int s, char *filename)
{
   char buffer[1024], numBytes[5];
   int fileHandle, done = FALSE, pos;

   if ((fileHandle = open(filename, O_RDONLY)) == -1)
      return FALSE;

   while (!done) {
      memset(buffer, 32, 1024);
      pos = read(fileHandle, buffer+4, 1020);

      if (pos < 1020) 
         done = TRUE;
      else if (pos == -1) 
         return FALSE;

      sprintf(numBytes, "%0004d", pos);
      strncpy(buffer, numBytes, 4);

      if (send(s, buffer, 1024, 0) != 1024) {
         close(fileHandle);
         return FALSE;
      }
   }

   close(fileHandle);

   return TRUE;
}

int ReceiveData(int s, char *buffer, int bufSize, int flags)
{
   return recvdata(s, buffer, bufSize, flags);    /** 50128 */
}

int GetVersion(wesco_socket_t s, char * version, const char * progName, const char * hostName)
{
   return WescoGetVersion(s, version, progName, hostName);
}
#endif      /* !defined(_WIN32) */
