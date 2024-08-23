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
#include <limits.h>

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

int CreateUserPartition(char *, char *, char *);
int GetPartition(char *, char *, char *);
void DeletePartition(char *);
int wzd_symlink(char *, char *);
int wzd_file_exists(char *);
void wzd_touch_file(char *);
int wzd_get_user_directory(char *, char *, char *);
void wzd_delete_partition(char *);

/*
 ******************************************************************************
 *
 * Function:    CreateUserPartition
 *
 * Description: This function creates a user partition.  It is used by the new
 *              WESNETzd system.
 *
 * Parameters:  partNumber - The partition number for the user directory.
 *              cobolName  - The name to append onto the path name.
 *              dir2       - The full path of the created directory.
 *
 * Returns:     TRUE if partition is created, else FALSE.
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *
 ******************************************************************************
*/
int CreateUserPartition(char *partNumber, char *cobolName, char *dirName)
{
   int rv;
   char * dir3;
   char * dir4;
   char * languageCode;
   char source[_POSIX_PATH_MAX];
   char target[_POSIX_PATH_MAX];

   rv = wzd_get_user_directory(partNumber, cobolName, dirName);
   if (rv == TRUE) {
      /* create the files required by WESNETzd applications */
     languageCode = getenv("LANGCODE");
     if (languageCode == NULL) {
        languageCode = "E";
     }

     dir3 = getenv("dir3");
     if (dir3 == NULL) {
        dir3 = "/IMOSDATA/5";
     }

     dir4 = getenv("dir4");
     if (dir4 == NULL) {
        dir4 = "/IMOSDATA/2";
     }
   }

   snprintf(source, sizeof(source), "%s/XMMPC%s", dir3, partNumber);
   snprintf(target, sizeof(target), "%s/XMMPC", dirName);
   if (!wzd_file_exists(source)) {
      wzd_touch_file(source);
   }

   wzd_symlink(source, target);

   snprintf(source, sizeof(source), "%s/ACTF-CRT%s", dir3, partNumber);
   snprintf(target, sizeof(target), "%s/ACTF-CRT", dirName);
   if (!wzd_file_exists(source)) {
      wzd_touch_file(source);
   }

   wzd_symlink(source, target);
}

/*
*******************************************************************************
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
* History:     08/11/05 mjb 05078 - Removed the logic that obtains the
*                                   partition number and creates the directory.
*                                   It was moved to function
*                                   wzd_get_user_directory.
*
*******************************************************************************
*/
int GetPartition(char *partNumber, char *cobolName, char *dirName)
{
	char lnXMFile[256], lnXmmfdl[256];

#if !defined(NO_LINK_XM)
	char *xmFile = "/IMOSDATA/5/XMFILE";
	char *xmmfdl = "/IMOSDATA/5/XMMFDL";
#endif /* !defined(NO_LINK_XM) */

	if (wzd_get_user_directory(partNumber, cobolName, dirName) == TRUE) {
		if (chdir(dirName)) {
			rmdir(dirName);
			return FALSE;
		}

#ifndef NO_LINK_XM
		sprintf(lnXMFile, "%s/XMFILE", dirName);
		sprintf(lnXmmfdl, "%s/XMMFDL", dirName);

		if (symlink(xmFile, lnXMFile)) {
			rmdir(dirName);
			return FALSE;
		}

		if (symlink(xmmfdl, lnXmmfdl)) {
			remove(lnXMFile);
			rmdir(dirName);
			return FALSE;
		}
			
		return TRUE;
	}
#endif    /* __hpux */

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

/*
 ******************************************************************************
 * 
 * Function:    wzd_symlink
 *
 * Description: This function creates a symbolic link to a file.
 *
 * Parameters:  src    - The file that is being linked to.
 *              target - The path where the link is created.
 *
 * Returns:     0 if successful, 1 if it fails
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *
 ******************************************************************************
*/
int wzd_symlink(char * src, char * target)
{
   return symlink(src, target);
}

/*
 ******************************************************************************
 * 
 * Function:    wzd_file_exists
 *
 * Description: This function checks to see if a file exists.
 *
 * Parameters:  fileName - The file whose existance is being tested.
 *
 * Returns:     TRUE if file exists, else FALSE.
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *
 ******************************************************************************
*/
int wzd_file_exists(char * fileName)
{
   int fd;

   if ((fd = open(fileName, O_RDONLY)) > -1) {
      close(fd);
      return TRUE;
   }
   else {
      return FALSE;
   }
}

/*
 ******************************************************************************
 * 
 * Function:    wzd_touch_file
 *
 * Description: This function creates the specified file.
 *
 * Parameters:  fileName - The file to create.
 *
 * Returns:     Nothing.
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *
 ******************************************************************************
*/
void wzd_touch_file(char * fileName)
{
   int fd;

   fd = open(fileName, O_RDONLY|O_CREAT, ALLPERMS);
   close(fd);
}

/*
 ******************************************************************************
 * 
 * Function:    wzd_get_user_directory
 *
 * Description: This function obtains the partition number and creates a user
 *              partition.
 *
 * Parameters:  partNumber - The partition number for the user directory.
 *              cobolName  - The name to append onto the path name.
 *              dir2       - The full path of the created directory.
 *
 * Returns:     TRUE if partition is created, else FALSE.
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *              08/22/06 mjb 05078 - Added logic to close the /IMOSDATA/USER
 *                                   directory.
 *
 ******************************************************************************
*/
int wzd_get_user_directory(char * partNumber, char * cobolName, char * dir2)
{
	int num, *numInUse, count, semID;
	char temp[7];
	DIR *dir;
	key_t key;
	struct dirent *entry;
	char *userDir;
	struct sembuf lockDir[] = {
		{ 0, 0, SEM_UNDO },          /* Wait for semaphore #0 = 0 */
		{ 0, 1, SEM_UNDO }           /* Increment semaphore #1    */
	};

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

        closedir(dir);

	/* 
		Loop through the array until an unused partition number is
		discovered.  Create a directory with that partition number as the
		first 2 characters followed by the COBOL program name.  Link the
		XMFILE and XMMFDL files into the directory.  Reset the semaphore
		to 0 and return TRUE.
	*/
	for (count = 0; count < MAXPARTITIONS; count++) {
		if (TESTBIT(numInUse, count) == 0) {
			sprintf(temp, "%03d", count);
			sprintf(dir2, "%s/%s%s", userDir, temp, cobolName);
			if (mkdir(dir2, S_IRWXU | S_IRWXG | S_IRWXO)) {
				free(numInUse);
				return FALSE;
			}

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
 ******************************************************************************
 * 
 * Function:    wzd_touch_file
 *
 * Description: This function creates the specified file.
 *
 * Parameters:  fileName - The file to create.
 *
 * Returns:     Nothing.
 * 
 * History:     08/11/05 mjb 05078 - Initial Development.
 *
 ******************************************************************************
*/
void wzd_delete_partition(char * dirName)
{
   int nResult;
   int nAttempt = 1;

   if (*dirName == 0)
   	return;

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
