#ifndef _COMMON_H_
#define _COMMON_H_
#define __COMMONH__

/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Created new constants for bonded inventory.
 ******************************************************************************
*/

#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>

#if !defined(_HPUX_SOURCE)
#   include <sys/bitypes.h>
#endif

#ifdef SIGCLD
#if (defined(SIGCHLD) && (SIGCLD == SIGCHLD)) || !defined(SIGCHLD)
#define SVR4
#endif
#endif

#ifdef NETWORKLOG
#undef NETWORKLOG
#endif

#define NETWORKLOG    (-1)

#if defined(__socklen_t_defined)
#	define SOCKLEN_T socklen_t
#elif defined(__STDC__)
#	if defined(_XOPEN_SOURCE) && (_XOPEN_SOURCE_EXTENDED - 0 >= 1)
#		if (_UNIX98_SOURCE - 0 >= 1)
#			define SOCKLEN_T socklen_t
#		else
#			define SOCKLEN_T size_t
#		endif
#	else
#		define SOCKLEN_T size_t
#	endif
#else
#	define SOCKLEN_T int
#endif

struct sockaddr_in;
struct hostent;

#if defined(_WIN32)
typedef int32_t mode_t;
typedef int32_t uid_t;
typedef int32_t gid_t;
#endif

#if defined(_WIN32)
#   define WS_NETWORK_ERROR_LOG "genserver.log"
#elif defined(_HPUX_SOURCE)
#       if defined(DPSPRO)
#               define WS_ROOT_DIR      "/dps"
#       else
#               define WS_ROOT_DIR      "/home"
#       endif
#       define WS_DEFAULT_APP_PATH      "/dps/genserver"
#       define WS_DEFAULT_DATA_PATH     "/dps/genserver"
#       define WS_DEFAULT_CFG_FILE      "/dps/genserver/wescom.ini"
#       define WS_NETWORK_ERROR_LOG     "/dps/genserver/genserver.log"
#else
#       define WS_DEFAULT_APP_PATH      "/IMOSDATA/5"
#       define WS_DEFAULT_DATA_PATH     "/IMOSDATA/2"
#       define WS_DEFAULT_CFG_FILE      "/IMOSDATA/2/wescom.ini"
#       define WS_NETWORK_ERROR_LOG     "/IMOSDATA/2/WESDOC.error.log"
#endif

#define CUSTOMERLENGTH          5
#define RECORDLENGTH            57
#define BRANCHLENGTH            4
#define QUANTITYLENGTH          10
#define HOSTNAMELENGTH          8
#define USERNAMELENGTH          12
#define FILENAMELENGTH          256
#define SIMLENGTH               11
#define COUNTERLENGTH           4
#define STATUSLENGTH            2
#define TRANSACTIONSIZE         8
#define TRANSFILERECLEN         11
#define RETNAMELENGTH           10
#define RETDESCLENGTH           15
#define DETAILLENGTH            512
#define SKULENGTH               11
#define BOLENGTH                10
#define ERRORNAMELENGTH         10
#define ERRORDESCLENGTH         15
#define QTYONHANDLENGTH         8
#define QTYONRESERVELENGTH      8
#define QTYONBACKORDERLENGTH    8
#define QTYONORDERLENGTH        8
#define AVERAGECOSTLENGTH       8
#define UOMLENGTH               1
#define PORECORDLENGTH          99
#define MODELENGTH              1
#define PARTITIONLENGTH         3
#define CLIENTNAMELENGTH        8
#define OPMODELENGTH            2
#define HOSTAREALENGTH          256
#define HOSTPROCESSLENGTH       256
#define PERMISSIONSLENGTH       4
#define OWNERLENGTH             12
#define GROUPLENGTH             12
#define FUNCNAMELENGTH          32
#define ISTSFLGLENGTH           1
#define VERSIONLENGTH           9
#define PASSWORDLENGTH          12

#define GOODCONVERSION          0
#define STRING_TOO_LARGE        1

#define DIRECTORY_EXISTS        0
#define DIRECTORY_CREATED       1
#define DIRECTORY_ERROR         2

#define WESCO_FILE_PACKET_TOTAL_SIZE	1024
#define WESCO_FILE_PACKET_DATA_SIZE		1020
#define WESCO_FILE_PACKET_INFO_SIZE		   4
#define WESCO_FILE_PACKET_PRINTF_FMT	"%0004d"
#define WESCO_FILE_PACKET_SCANF_FMT		"%04d"

#define WESCOM_INI_FILE					WS_DEFAULT_CFG_FILE
#define WESCOM_LOCK_FILE            "/tmp/LOCK.wescom"

#define WESCOMINI                   WESCOM_INI_FILE

#define WS_CFG                      "WS_CFG"

#define DBGPERM (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#define ALLPERM (S_IRWXU|S_IRWXG|S_IRWXO)
#define READWRITE DBGPERM

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char hostname[HOSTNAMELENGTH];
    char username[USERNAMELENGTH];
    char partition[PARTITIONLENGTH];
    char status[STATUSLENGTH];
    char retname[RETNAMELENGTH];
    char retdesc[RETDESCLENGTH];
    char details[DETAILLENGTH];
} FUNCPARMS, *LPFUNCPARMS;

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char hostname[HOSTNAMELENGTH];
    char username[USERNAMELENGTH];
    char partition[2];
    char status[STATUSLENGTH];
    char retname[RETNAMELENGTH];
    char retdesc[RETDESCLENGTH];
    char details[DETAILLENGTH];
} OLDFUNCPARMS, *LPOLDFUNCPARMS;

typedef struct {
    char name[ERRORNAMELENGTH];
    char description[ERRORDESCLENGTH];
} ERRORSTR, *LPERRORSTR;

typedef struct {
    char name[FILENAMELENGTH];
    char permissions[PERMISSIONSLENGTH];
    char owner[OWNERLENGTH];
    char group[GROUPLENGTH];
} REMOTEFILE, *LPREMOTEFILE;

char * get_cfg_file(char *, size_t);
void ParseArguments(int, char *[]);
void CommandLineHelp(char *);
void ISONNET(LPFUNCPARMS);
int WescoTransmitFile(wesco_socket_t, char *);
int ReceiveFile(wesco_socket_t, const char *);
bool_t ParseFilename(const char *, char *, char *);
int CtoCobol(char *, const char *, int);
int CobolToC(char *, const char *, int, int);
int FileSend(wesco_socket_t, REMOTEFILE);
int FileReceive(wesco_socket_t, REMOTEFILE);
int FileMultiSend(wesco_socket_t, REMOTEFILE);
int FileMultiReceive(wesco_socket_t, REMOTEFILE);
int CheckDirectory(const char *);
void NetworkError(const char *, int);
void SendLogImmediately(void);
void TCPAlarm(int);
int AuthorizeConnection(wesco_socket_t, struct hostent **, struct sockaddr_in *);
int WescoGetVersion(wesco_socket_t, char *, const char *, const char *);
char *getqueuefilename(char *, char *);
int rsvport(unsigned short *);
char * ws_basename(char *);
char * ws_dirname(char *);
void wescolog(int fd, char *fmt, ...);
void locallog(int, char *, ...);
struct hostent *ws_gethostbyname(char *);

/* The VERSIONCONTROL structure contains information for each version
   that is supported.  The number is a string representation of the
   version number.  The Server is a pointer to the server function
   this version uses. */
struct SERVERVERSIONCONTROL {
  char number[VERSIONLENGTH];
  void (* Server)(char *, char *, int);
  char cobolProgram[255];
  int bufferSize;
};

struct CLIENTVERSIONCONTROL {
  char number[VERSIONLENGTH];
  void (* Client)(int, FUNCPARMS*, int);
  int bufferSize;
};

struct VERSIONINFO {
  int release;
  int major;
  int minor;
};

typedef struct SERVERVERSIONCONTROL SERVERVERSIONCONTROL;
typedef struct CLIENTVERSIONCONTROL CLIENTVERSIONCONTROL;
typedef struct VERSIONINFO VERSIONINFO;

#if defined(unix) || defined(__hpux)
typedef int socket_t;
#endif    /* unix */

#if defined(__hpux) || defined(_GNU_SOURCE)
#   define file_pattern_match(s,p)      (fnmatch(p, s, 0) == 0)
#else
#   define file_pattern_match(s,p)      (gmatch(s, p) != 0)
#endif    /* __hpux */

struct ws_pfent {
	char pf_name[10];
	char pf_passwd[30];
	char pf_start[50];
	char pf_langcode;
	int32_t pf_lastaccess;
	int32_t pf_lastchange;
	int32_t pf_attempts;
	char pf_sflag;
};

#endif    /* _COMMON_H_ */
