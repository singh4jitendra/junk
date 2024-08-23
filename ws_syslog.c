#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <time.h>

#include "ws_syslog.h"

int use_syslog;
struct _ws_log_t * wsl = NULL;

void ws_openlog(char * ident, int logopt, int facility, int primask)
{
	char inifile[PATH_MAX + 1];
	char logfile[PATH_MAX + 1];

	memset(inifile, 0, sizeof(inifile));
	memset(logfile, 0, sizeof(logfile));

	get_cfg_file(inifile, sizeof(inifile));

	use_syslog = GetPrivateProfileInt(WS_LOG_INI_SECTION, WS_LOG_USE_SYSLOG_KEY,
		WS_LOG_USE_SYSLOG_DEFAULT, inifile);
	if (use_syslog) {
		openlog(ident, logopt, facility);
		setlogmask(primask);
	}
	else {
		if (wsl == NULL) {
			wsl = (struct _ws_log_t *)malloc(sizeof(struct _ws_log_t));
			if (wsl != NULL) {
				GetPrivateProfileString(WS_LOG_INI_SECTION, WS_LOG_FILE_KEY,
					logfile, sizeof(logfile), WS_LOG_FILE_DEFAULT, inifile);

				wsl->ident = ident;
				wsl->fp = fopen(logfile, "a");
				wsl->logopt = logopt;
				wsl->level = primask;
			}
		}
	}

	return;
}

void ws_syslog(int level, char * msg, ...)
{
	va_list ap;
	char buf[4096];
	struct flock wrlock;

	memset(buf, 0, sizeof(buf));

	va_start(ap, msg);

	if (use_syslog) {
		vsnprintf(buf, sizeof(buf), msg, ap);
		syslog(level, buf);
	}
	else {
		if (wsl != NULL) {
			if ((wsl->level & LOG_MASK(level)) || (level == LOG_EMERG)) {
				ws_timestamp(buf, sizeof(buf));
				if ((wsl->ident == NULL) || (*(wsl->ident) == (char)0)) {
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s",
						"????????");
				}
				else {
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s",
						wsl->ident);
				}

				if (wsl->logopt & LOG_PID) {
					snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "[%d]",
						getpid());
				}

				strcat(buf, ": ");
				vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), msg, ap);

				/* place a write lock on the file */
				wrlock.l_type = F_WRLCK;
				wrlock.l_whence = 0;
				wrlock.l_start = (off_t)0;
				wrlock.l_len = (off_t)0;
				if (fcntl(fileno(wsl->fp), F_SETLKW, &wrlock) < 0) {
					return;
				}

				fprintf(wsl->fp, "%s\n", buf);
				fflush(wsl->fp);

				wrlock.l_type = F_UNLCK;
				fcntl(fileno(wsl->fp), F_SETLK, &wrlock);
			}
		}
	}

	va_end(ap);

	return;
}

int ws_setlogmask(int maskpri)
{
	int rv;

	if (use_syslog) {
		rv = setlogmask(maskpri);
	}
	else {
		rv = wsl->level;
		wsl->level = (wsl->level & LOG_FACMASK) | maskpri;
	}

	return rv;
}

void ws_closelog(void)
{
	if (use_syslog) {
		closelog();
	}
	else {
		if (wsl != NULL) {
			fclose(wsl->fp);
			free(wsl);
			wsl = NULL;
		}
	}

	return;
}

/*
 ******************************************************************************
 *
 * Name:        ws_timestamp
 *
 * Description: This function places the current time using a configurable
 *              format in a buffer.
 *
 * Parameters:  buffer   - Memory area to store the time stamp.
 *              bufsize  - Size of the buffer.
 *
 * Returns:     None.
 *
 * Modified:    05/24/05 04041 - Added logic to set the value of the
 *                               initialization file.
 ******************************************************************************
*/
void ws_timestamp(char * buffer, size_t bufsize)
{
	char inifile[PATH_MAX + 1];
	char timefmt[PATH_MAX];
	time_t current_time;
	struct tm * tmptr;

	if ((buffer == NULL) || (bufsize < (size_t)20))
		return;

	memset(inifile, 0, sizeof(inifile));
	memset(timefmt, 0, sizeof(timefmt));

	get_cfg_file(inifile, sizeof(inifile));

	time(&current_time);
	tmptr = localtime(&current_time);

	if (tmptr != NULL) {
		GetPrivateProfileString(WS_LOG_INI_SECTION, WS_LOG_TIME_FMT_KEY,
			timefmt, sizeof(timefmt), WS_LOG_TIME_FMT_DEFAULT, inifile);

		strftime(buffer, bufsize, timefmt, tmptr);
	}

	return;
}
