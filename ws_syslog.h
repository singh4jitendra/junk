#if !defined(_WS_SYSLOG_H)
#define _WS_SYSLOG_H

#include <syslog.h>

struct _ws_log_t {
	FILE * fp;
	int level;
	int logopt;
	char * ident;
};

void ws_openlog(char * ident, int logopt, int facility, int primask);
void ws_syslog(int level, char * msg, ...);
int ws_setlogmask(int maskpri);
void ws_closelog(void);
void ws_timestamp(char * buffer, size_t bufsize);

#if !defined(WS_DEFAULT_DATA_PATH)
#include "common.h"
#endif

#define WS_LOG_INI_SECTION        "logging"
#define WS_LOG_USE_SYSLOG_KEY     "use-syslog"
#define WS_LOG_USE_SYSLOG_DEFAULT 0
#define WS_LOG_FILE_KEY           "log-file"

#if defined(_DPSPRO)
#define WS_LOG_FILE_DEFAULT       "/dps/genserver/ws_system.log"
#else
#define WS_LOG_FILE_DEFAULT       "/IMOSDATA/5/ws_system.log"
#endif

#define WS_LOG_FILE_DEFAULT       "/IMOSDATA/5/ws_system.log"
#define WS_LOG_TIME_FMT_KEY       "timestamp-format"
#define WS_LOG_TIME_FMT_DEFAULT   "%m/%d/%Y %H:%M:%S"

#endif
