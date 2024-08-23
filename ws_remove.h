#if !defined(_WS_REMOVE_H)
#define _WS_REMOVE_H

/* define's for ws_remove() return status */
#define WS_REMOVE_FAILED				-1
#define WS_REMOVE_SUCCESSFUL	 		 0

/* define's for ws_remove() flags */
#define WS_REMOVE_SUBDIR				0x0001
#define WS_REMOVE_SELF					0x0002

#define WS_REMOVE_DEFAULT_PATTERN	"*"

#define WS_REMOVE_TRACE_KEY         "ws_remove"

int ws_remove(char * lpDirName, char * lpPattern, short nFlags);

#endif
