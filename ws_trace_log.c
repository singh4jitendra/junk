#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "ws_trace_log.h"

int ws_trace_log(char * lpKey, int nLevel, char * lpFormat, ...)
{
	FILE * fp;
	int nLogLevel;
	char szTime[50];
	struct tm * tmTime;
	va_list vaArguments;
	time_t current_time;
	char szMessage[512];
	char szFileName[FILENAMELENGTH+1];

	nLogLevel = GetPrivateProfileInt("trace", lpKey, 0, WESCOM_INI_FILE);
	if (nLogLevel != 0) {
		if (nLevel <= nLogLevel) {
			GetPrivateProfileString("trace", "file", szFileName,
				sizeof(szFileName), "/IMOSDATA/2/trace.log", WESCOM_INI_FILE);

			if ((fp = fopen(szFileName, "a")) == (FILE *)NULL)
				return -1;

			time(&current_time);
			tmTime = localtime(&current_time);
			strftime(szTime, sizeof(szTime), "%m/%d/%Y  %H:%M:%S", tmTime);

			sprintf(szMessage, "%s : %d: ", szTime, getpid());

			va_start(vaArguments, lpFormat);
			vsprintf(szMessage+strlen(szMessage), lpFormat, vaArguments);
			fwrite(szMessage, sizeof(char), strlen(szMessage), fp);
			va_end(vaArguments);

			fclose(fp);
		}
	}

	return 0;
}
