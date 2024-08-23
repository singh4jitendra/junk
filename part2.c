#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "ws_remove.h"

long base2(int);

#define SETBIT(x,y) (x)[(y)/8] |= (base2((y) % 8))
#define TESTBIT(x,y) ((x)[(y)/8] & (base2((y) % 8)))

/*
******************************************************************************
*
*  To change the value of MAXPARTITONS use the compiler option to define a
*  preprocessor variable.  ex:
*    cc -DMAXPARTITIONS=1000
*
*  The cc compiler used in UNIX allows a preprocessor variable to be defined
*  on the command line with the -D option.
*
******************************************************************************
*/
#ifndef MAXPARTITIONS
#define MAXPARTITIONS 1000
#endif

int GetPartition(char *, char *, char *);
void DeletePartition(char *);

/*
****************************************************************************
*
* Function:    GetPartition
*
* Description: This function checks the IMOSDATA/USER directory to find
*              the next available partition number.  If a number is 
*              available a directory is created for it.
*
* Parameters:  partNumber - Pointer to partition number
*              cobolName  - Name of the COBOL function
*              dirName    - Full name of the partition directory
*
* Returns:     TRUE if directory is created; else FALSE
*
****************************************************************************
*/
int GetPartition(char *partNumber, char *cobolName, char *dirName)
{
	int num, *numInUse, count, semID;
	char lnXMFile[256], lnXmmfdl[256];
	char temp[7];
	DIR *dir;
	key_t key;
	struct dirent *entry;
	char *userDir;
	struct sembuf lockDir[] = {
		{ 0, 0, SEM_UNDO },          /* Wait for semaphore #0 = 0 */
		{ 0, 1, SEM_UNDO }           /* Increment semaphore #1    */
	};

#if !defined(NO_LINK_XM)
	char *xmFile = "/IMOSDATA/5/XMFILE";
	char *xmmfdl = "/IMOSDATA/5/XMMFDL";
#endif /* !defined(NO_LINK_XM) */

	if ((userDir = getenv("USERDIR_ROOT")) == NULL)
#if defined(__hpux)
	userDir = "/edi/xlate";
#else
	userDir = "/IMOSDATA/USER";
#endif

	if ((numInUse = calloc(MAXPARTITIONS, sizeof(char))) == NULL)
		return FALSE;

	/*
		Create a token based on /IMOSDATA/USER for the semaphore.  Check
		to see if the semaphore exists.  If not, then create it.  Otherwise
		wait until it is 0 before continuing.  Change the semaphore to 1
		when continuing to indicate other processes should wait.
	*/
	key = ftok(userDir, 25);
	semID = semget(key, 1, IPC_CREAT | 0666);
	semop(semID, lockDir, 2);

	/* open the /IMOSDATA/USER directory */
	if ((dir = opendir(userDir)) == NULL) {
		perror(userDir);
		semctl(semID, 0, SETVAL, 0);
		return FALSE;
	}

	/*
		Read each entry in the directory.  If the first two characters in
		the entry are numeric, get the value of the numbers.  Increment
		the corresponding array element to indicate that partition number
		is used.
	*/
	while ((entry = readdir(dir)) != NULL) {
		if (sscanf(entry->d_name, "%003d", &num)) {
			SETBIT(numInUse, num);
		}
	}

	/* 
		Loop through the array until an unused partition number is
		discovered.  Create a directory with that partition number as the
		first 2 characters followed by the COBOL program name.  Link the
		XMFILE and XMMFDL files into the directory.  Reset the semaphore
		to 0 and return TRUE.
	*/
	for (count = 0; count < MAXPARTITIONS; count++) {
		if (TESTBIT(numInUse, count) == 0) {
			snprintf(temp, sizeof(temp), "%03d", count);
			snprintf(dirName, sizeof(dirName), "%s/%s%s", userDir, temp, cobolName);
			if (mkdir(dirName, S_IRWXU | S_IRWXG | S_IRWXO)) {
				free(numInUse);
				return FALSE;
			}

			if (chdir(dirName)) {
				free(numInUse);
				rmdir(dirName);
				return FALSE;
			}

#ifndef NO_LINK_XM
			snprintf(lnXMFile, sizeof(lnXMFile), "%s/XMFILE", dirName);
			snprintf(lnXmmfdl, sizeof(lnXmmfdl), "%s/XMMFDL", dirName);

			if (symlink(xmFile, lnXMFile)) {
				free(numInUse);
				rmdir(dirName);
				return FALSE;
			}

			if (symlink(xmmfdl, lnXmmfdl)) {
				free(numInUse);
				remove(lnXMFile);
				rmdir(dirName);
				return FALSE;
			}
#endif    /* __hpux */

			strncpy(partNumber, temp, PARTITIONLENGTH);
			semctl(semID, 0, SETVAL, 0);

			free(numInUse);

			return TRUE;
		}
	}

	/*
		No partition numbers are available.  Set the semaphore to 0 and
		return FALSE.
	*/
	semctl(semID, 0, SETVAL, 0);
	free(numInUse);

	return FALSE;
}

/*
****************************************************************************
*
* Function:    DeletePartition
*
* Description: This function deletes the partition directory created for
*              a process.
*
* Parameters:  dirName    - Full name of the partition directory
*
* Returns:     Nothing
*
****************************************************************************
*/
void DeletePartition(char *dirName)
{
	int nResult;
	int nAttempt = 1;

	if (*dirName == 0)
		return;

	chdir("..");

	nResult = ws_remove(dirName, NULL, WS_REMOVE_SELF|WS_REMOVE_SUBDIR);
	while ((nResult != WS_REMOVE_SUCCESSFUL) && (nAttempt <= 3)) {
		wescolog(NETWORKLOG, "DeletePartition: ws_remove: Attempt %d failed",
			nAttempt);
		sleep(1);
		nAttempt++;
		nResult = ws_remove(dirName, NULL, WS_REMOVE_SELF|WS_REMOVE_SUBDIR);
	}

	if ((nResult != WS_REMOVE_SUCCESSFUL) && (nAttempt >= 3)) {
		wescolog(NETWORKLOG, "DeletePartition: ws_remove: Failed to remove "
			"directory");
	}
}
