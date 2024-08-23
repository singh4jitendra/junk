#ifndef __TERMCMDSH__
#define __TERMCMDSH__

struct termcmds {
	char * tc_term;
	char *tc_clear;
	char *tc_rmso;
};

/* TERM types */
#define TC_7902_4902_TERM "7902"
#define TC_VT100_TERM     "vt100"
#define TC_ADDS_TERM      "4902"
#define TC_AT386_TERM     "at386"

/* Terminal commands */
#define NCR_7902_CLEAR    "\x0c"
#define NCR_7902_RMSO     "\x1b\x30\x40"
#define VT100_CLEAR       "\x1b\x5bH\x1b\x5bJ"
#define VT100_RMSO        "\x1b\x5b\x6d"
#define ADDS_CLEAR        "\x0c"
#define ADDS_RMSO         "\x1b\x30\x40"
#define AT386_CLEAR       "\x1b\x5b\x32\x4a\x1b\x5b\x4b"
#define AT386_RMSO        "\x1b\x5b\x6d"

#define TC_NCR_7902_INDEX 0
#define TC_VT100_INDEX   1
#define TC_ADDS_INDEX    2
#define TC_AT386_INDEX   3

#endif
