#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "common.h"

/*
 ******************************************************************************
 *
 * Function:    wescolog
 *
 * Description: This function writes a message out to a log file.  If the
 *              value of fd is NETWORKLOG the WESDOC.error.log file is used;
 *              otherwise, fd is the file descriptor for the log file.
 *              This function works like a printf when accepting a format
 *              string and variable number of parameters.
 *
 * Parameters:  fd  - file descriptor of log file to write to
 *              fmt - format string for the vsprintf statement
 *              ... - variable parameters for the vsprintf statement
 *
 * Returns:     nothing
 *
 ******************************************************************************
*/
void wescolog(int fd, char *fmt, ...)
{
    va_list ap;
    char buffer[256];

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    if (fd == NETWORKLOG)
        NetworkError(buffer, TRUE);
    else
        if (writetolog(fd, buffer, strlen(buffer), TRUE) < 0)
            err_ret("lock failed");
    va_end(ap);
}

void locallog(int fd, char *fmt, ...)
{
	va_list ap;
	char msg[512];
	char timeStr[50];
	time_t currentTime;
	struct tm *timePtr;

	/* get current time */
	time(&currentTime);
	timePtr = localtime(&currentTime);
	strftime(timeStr, sizeof(timeStr), "%m/%d/%Y  %H:%M:%S", timePtr);

	snprintf(msg, sizeof(msg), "%s : %d: ", timeStr, getpid());

	/* write message passed in through parameters */
	va_start(ap, fmt);
	vsprintf(msg+strlen(msg), fmt, ap);
	write(fd, msg, strlen(msg));
	va_end(ap);
}

int ws_packet_hex_dump(FILE *fp, char *buffer, size_t len)
{
	int i;
	union {
		char b[2];
		short s;
	} ud;

	ud.s = 0;
	for (i = 0; i < len; i++) {
		if (!(i%25))
			fprintf(fp, "\n");

		ud.b[0] = buffer[i];
		fprintf(fp, "%02hx ", ud.s);
	}

	fprintf(fp, "\n");

	return 0;
}
