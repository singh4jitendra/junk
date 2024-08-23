#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

#include "common.h"
#include "ws_remove.h"
#include "ws_trace_log.h"

/*
 ******************************************************************************
 *
 * Function Name: ws_remove
 *
 * Description:   This function removes files and directories based on the
 *                pattern supplied as a parameter to the function.  If a NULL
 *                is passed in the pattern (lpPattern) parameter, ws_remove
 *                uses a default pattern of "*".  The function fails if a
 *                NULL is passed in the directory name (lpDirName) parameter.
 *                Two flags can be provided to control how the function
 *                processes the request:
 *                  WS_REMOVE_SUBDIR - Specifies the function should
 *                                     recursively remove sub-directories
 *                                     that match lpPattern.  Other files
 *                                     that match lpPattern will be removed
 *                                     as well.
 *                  WS_REMOVE_SELF   - Specifies the function should remove
 *                                     the directory specified by lpDirName.
 *
 *                Examples:
 *                  To remove files only:
 *                    ws_remove("/tmp", "some*pattern", 0);
 *
 *                  To remove sub-directories:
 *                    ws_remove("/tmp", "some_dir", WS_REMOVE_SUBDIR);
 *
 *                  To remove empty directory:
 *                    ws_remove("/tmp", NULL, WS_REMOVE_SELF);
 *
 *                  To remove full directory:
 *                    ws_remove("/tmp", NULL, WS_REMOVE_SELF|WS_REMOVE_SUBDIR);
 *
 * Parameters:    lpDirName - Pointer to a character string that specifies the
 *                            name of the directory.  May not be NULL.
 *                lpPattern - Pointer to a character string that specifies the
 *                            the pattern to use while deleting files.  If
 *                            NULL, a default of "*" is used.
 *                nFlags    - Short integer used to control how directories
 *                            treated while removing files.  Valid flags are
 *                               WS_REMOVE_SUBDIR - Recursively remove sub-
 *	                                                 directories.
 *                               WS_REMOVE_SELF   - Remove directory specified
 *                                                   by lpDirName.
 *                            This field may be a combination of the two flags
 *                            or a zero to specify no flags are to be used.
 *
 * Returns:       WS_REMOVE_SUCCESSFUL on success, WS_REMOVE_FAILED on an
 *                error.
 *
 * Created:       mjb   G.T. Systems   12/14/99
 *
 * Modifications:
 *
 ******************************************************************************
*/
int ws_remove(char * lpDirName, char * lpPattern, short nFlags)
{
	int nResult;
	DIR * dpDirectory;
	struct stat stFileStat;
	struct dirent *depEntry;
	char szFileName[FILENAMELENGTH+1];
	int nReturnCode = WS_REMOVE_SUCCESSFUL;

	if ((lpDirName == (char *)NULL) || (*lpDirName == 0)) {
		ws_trace_log(WS_REMOVE_TRACE_KEY, 5,
			"ws_remove: NULL passed for directory name: bailing\n");
		return WS_REMOVE_FAILED;
	}

	ws_trace_log(WS_REMOVE_TRACE_KEY, 5, "ws_remove: process directory %s\n",
		lpDirName);

	if (lpPattern == (char *)NULL) {
		lpPattern = WS_REMOVE_DEFAULT_PATTERN;
		ws_trace_log(WS_REMOVE_TRACE_KEY, 5,
			"ws_remove: NULL passed for pattern: default %s\n",
			lpPattern);
	}

	dpDirectory = opendir(lpDirName);
	if (dpDirectory == (DIR *)NULL) {
		ws_trace_log(WS_REMOVE_TRACE_KEY, 1,
			"ws_remove: opendir(): %s: %s: bailing\n", lpDirName,
			strerror(errno));
		wescolog(NETWORKLOG, "ws_remove: opendir: %s: %s", lpDirName,
			strerror(errno));
		return WS_REMOVE_FAILED;
	}

	ws_trace_log(WS_REMOVE_TRACE_KEY, 5, "ws_remove: opendir(): successful\n");

	while ((depEntry = readdir(dpDirectory)) != (struct dirent *)NULL) {
		ws_trace_log(WS_REMOVE_TRACE_KEY, 1,
			"ws_remove: depEntry->d_name=%s\n", depEntry->d_name);

		if (!strcmp(depEntry->d_name, ".") || !strcmp(depEntry->d_name, "..")) {
			ws_trace_log(WS_REMOVE_TRACE_KEY, 9,
				"ws_remove: skipping %s\n", depEntry->d_name);
			continue;		/* ignore parent and self dot entries */
		}

		if (file_pattern_match(depEntry->d_name, lpPattern) != 0) {
			ws_trace_log(WS_REMOVE_TRACE_KEY, 9,
				"ws_remove: %s matches pattern %s\n", depEntry->d_name,
				lpPattern);

			sprintf(szFileName, "%s/%s", lpDirName, depEntry->d_name);

			nResult = stat(szFileName, &stFileStat);
			if (nResult != 0) {
				ws_trace_log(WS_REMOVE_TRACE_KEY, 1, "ws_remove: stat(): %s: %s\n",
					szFileName, strerror(errno));
				wescolog(NETWORKLOG, "ws_remove: stat: %s", strerror(errno));
				nReturnCode = WS_REMOVE_FAILED;
				continue;
			}

			if (S_ISDIR(stFileStat.st_mode)) {
				ws_trace_log(WS_REMOVE_TRACE_KEY, 9,
					"ws_remove: %s is a directory\n", szFileName);
				if (nFlags & WS_REMOVE_SUBDIR) {
					ws_trace_log(WS_REMOVE_TRACE_KEY, 9,
						"ws_remove: recursively remove sub-directory %s\n",
						szFileName);
					nResult = ws_remove(szFileName, "*",
						WS_REMOVE_SUBDIR|WS_REMOVE_SELF);
					if (nResult != 0) {
						ws_trace_log(WS_REMOVE_TRACE_KEY, 1,
							"ws_remove: failed to remove sub-directory %s\n",
							szFileName);
						nReturnCode = WS_REMOVE_FAILED;
					}
				}
			}
			else {
				ws_trace_log(WS_REMOVE_TRACE_KEY, 7, "ws_remove: remove file %s\n",
					szFileName);

				nResult = remove(szFileName);
				if (nResult != 0) {
					ws_trace_log(WS_REMOVE_TRACE_KEY, 1,
						"ws_remove: remove(): %s: %s\n", szFileName,
						strerror(errno));
					wescolog(NETWORKLOG, "ws_remove: remove: %s", strerror(errno));
					nReturnCode = WS_REMOVE_FAILED;
				}
			}
		}
		else
			ws_trace_log(WS_REMOVE_TRACE_KEY, 9,
				"ws_remove: %s does not match pattern %s",
				depEntry->d_name, lpPattern);
	}

	if (errno != 0)
		ws_trace_log(WS_REMOVE_TRACE_KEY, 1, "ws_remove: readdir(): %s: %s\n",
			lpDirName, strerror(errno));

	if ((nReturnCode == WS_REMOVE_SUCCESSFUL) && (nFlags & WS_REMOVE_SELF)) {
		ws_trace_log(WS_REMOVE_TRACE_KEY, 7,
			"ws_remove: attempt to remove directory %s\n", lpDirName);
		nResult = rmdir(lpDirName);
		if (nResult != 0) {
			ws_trace_log(WS_REMOVE_TRACE_KEY, 1, "ws_remove: rmdir(): %s: %s\n",
				lpDirName, strerror(errno));
			wescolog(NETWORKLOG, "ws_remove: rmdir: %s", strerror(errno));
			nReturnCode = WS_REMOVE_FAILED;
		}
	}

	ws_trace_log(WS_REMOVE_TRACE_KEY, 5, "ws_remove: close directory\n");
	closedir(dpDirectory);

	ws_trace_log(WS_REMOVE_TRACE_KEY, 5, "ws_remove: return %s\n",
		(nReturnCode == WS_REMOVE_FAILED) ? "WS_REMOVE_FAILED" :
			"WS_REMOVE_SUCCESSFUL");

	return nReturnCode;
}
