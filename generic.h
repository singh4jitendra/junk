#ifndef __GENERICH__
#define __GENERICH__

#ifndef _COMMON_H_
#include "common.h"
#endif

#define SWITCHLENGTH            12

#define SPECIFIEDPART_TRANS        1
#define SPECIFIEDPART_INFILE       2
#define SPECIFIEDPART_OUTFILE      3
#define SPECIFIEDPART_IOFILES      4
#define SPECIFIEDPART_MULTIN       5
#define SPECIFIEDPART_MULTOUT      6
#define SPECIFIEDPART_MULTIO       7
#define CREATEDPART_TRANS          8
#define CREATEDPART_INFILE         9
#define CREATEDPART_OUTFILE       10
#define CREATEDPART_IOFILES       11
#define CREATEDPART_MULTIN        12
#define CREATEDPART_MULTOUT       13
#define CREATEDPART_MULTIO        14
#define FILE_SEND                 15
#define FILE_RETRIEVE             16
#define FILE_SENDRETRIEVE         17
#define FILE_MULTISEND            18
#define FILE_MULTIRETRIEVE        19
#define FILE_MULTISENDRETRIEVE    20
#define GQ_SPECIFIEDPARTITION     21
#define GQ_SPECIFIEDPART_INFILE   22
#define GQ_SPECIFIEDPART_OUTFILE  23
#define GQ_SPECIFIEDPART_IOFILES  24
#define GQ_SPECIFIEDPART_MULTIN   25
#define GQ_SPECIFIEDPART_MULTOUT  26
#define GQ_SPECIFIEDPART_MULTIO   27
#define GQ_CREATEDPARTITION       28
#define GQ_CREATEDPART_INFILE     29
#define GQ_CREATEDPART_OUTFILE    30
#define GQ_CREATEDPART_IOFILES    31
#define GQ_CREATEDPART_MULTIN     32
#define GQ_CREATEDPART_MULTOUT    33
#define GQ_CREATEDPART_MULTIO     34
#define GQ_FILE_SEND              35
#define GQ_FILE_RETRIEVE          36
#define GQ_FILE_SENDRETRIEVE      37
#define GQ_FILE_MULTISEND         38
#define GQ_FILE_MULTIRETRIEVE     39
#define GQ_FILE_MULTISENDRETRIEVE 40

#define FIRST_GENQUEUE_MODE       GQ_SPECIFIEDPARTITION
#define LAST_GENQUEUE_MODE	       GQ_FILE_MULTISENDRETRIEVE

#define IS_QUEUED_MODE(x)	((x >= FIRST_GENQUEUE_MODE) && (x <= LAST_GENQUEUE_MODE))

#define GENSERVER_INI_SECTION	          "genserver"
#define GENSERVER_QPREFIX_KEY	          "queueprefix"
#define GENSERVER_SERVICE_KEY	          "service"
#define GENSERVER_TIMEOUT_KEY           "timeout"
#define GENSERVER_SNAPSHOT_ARGS_KEY     "num-snapshot-args"
#define GENSERVER_SNAPSHOT_PATH_KEY     "snapshot-path"
#define GENSERVER_DEFAULT_QPREFIX       "genqueue"
#define GENSERVER_DEFAULT_SERVICE       "WDOCGEN"
#define GENSERVER_DEFAULT_SNAPSHOT_ARGS (-1)
#define GENSERVER_DEFAULT_SNAPSHOT_PATH "/IMOSDATA/0"

#define GENCLIENT_INI_SECTION       "genclient"
#define GENCLIENT_LISTEN_TO_KEY     "listen-timeout"
#define GENCLIENT_DEFAULT_LISTEN_TO (120)

#define GEN_REG_KEY_RELEASE         "SOFTWARE\\GenServer 2000\\Releases"
#define GEN_REG_WESCOM_RELEASE      "Wescom Release"
#define GEN_REG_FUNCTION_NAME	      "Function Name"
#define GEN_REG_DLL_PATH            "Dll Path"
#define GENSERVER_COBOL_PROGRAM     "TRAFFICOP"

#define GS_CALL_COBOL_ERROR         "05"

#define GS_DEFAULT_TERM             "TERM=vt100"
#define GS_UNKNOWN_TERM             "TERM=unknown"

typedef struct {
    char transaction[TRANSACTIONSIZE];
    char hostname[HOSTNAMELENGTH];
    char clientname[CLIENTNAMELENGTH];
    char opmode[OPMODELENGTH];
    char username[USERNAMELENGTH];
    char partition[PARTITIONLENGTH];
    char status[STATUSLENGTH];
    ERRORSTR error;
    char hostarea[HOSTAREALENGTH];
    char hostprocess[HOSTPROCESSLENGTH];
    char clientsendfile[FILENAMELENGTH];
    REMOTEFILE hostrecvfile;
    char hostsendfile[FILENAMELENGTH];
    REMOTEFILE clientrecvfile;
    char redirect;
    char switches[SWITCHLENGTH];
    char details[2727];
} GENTRANSAREA, *LPGENTRANSAREA;

#if defined(_WIN32)
typedef int (CALLBACK *LPFNGENFUNC)(wesco_socket_t, int32_t *, LPGENTRANSAREA, int);

typedef struct GENVERSION {
	char version[VERSIONLENGTH+1];
	char function[FILENAMELENGTH+1];
	char dllpath[FILENAMELENGTH+1];
} GENVERSION, *LPGENVERSION;
#else
#   define _NUM_GS_VERSIONS_SUPPORTED 3

typedef int (*LPFNGENFUNC)(wesco_socket_t, int32_t *, LPGENTRANSAREA, int);

typedef struct GENVERSION {
	char version[VERSIONLENGTH+2];
	LPFNGENFUNC client_function;
	int heartbeat;
} GENVERSION, *LPGENVERSION;

int GenClient060503(wesco_socket_t, int32_t *, LPGENTRANSAREA, int);

#endif

#endif
