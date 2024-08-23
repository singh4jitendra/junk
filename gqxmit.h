#if !defined(_GQXMIT_H)
#define _GQXMIT_H

#if defined(__hpux)
#   define GQXMIT_QUEUE_ROOT_DEFAULT "/home/genserver/GQDIR"
#   define GQXMIT_DEBUG_FILE_DEFAULT "/home/genserver/debug.gqu"
#   define GQXMIT_LOG_FILE_DEFAULT   "/home/genserver/debug.gqu"
#else
#   define GQXMIT_QUEUE_ROOT_DEFAULT "/IMOSDATA/2/QUEUES/GQDIR"
#   define GQXMIT_DEBUG_FILE_DEFAULT "/IMOSDATA/5/debug.gqu"
#   define GQXMIT_LOG_FILE_DEFAULT   "/IMOSDATA/5/debug.gqu"
#endif

#define GQXMIT_INI_SECTION         "GQxmit"
#define GQXMIT_LOG_FILE_KEY        "logfile"
#define GQXMIT_DEBUG_FILE_KEY      "debugfile"
#define GQXMIT_DELETE_FILES_KEY    "deletefiles"
#define GQXMIT_TIMEOUT_KEY         "timeout"
#define GQXMIT_TIMEOUT_DEFAULT     "30"
#define GQXMIT_MAX_RETRIES_KEY     "maxretries"
#define GQXMIT_MAX_RETRIES_DEFAULT 5
#define GQXMIT_MAILTO_KEY          "mail-to"
#define GQXMIT_MAILTO_DEFAULT      "root"

void heartbeat(void);

#endif
