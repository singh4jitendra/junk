#ifndef _GEN2_H_
#define _GEN2_H_

#ifndef _ERRNO_H_
#include <errno.h>
#endif

/* opcodes */
#define _GEN_PUT     1       /* Send file to remote system                */
#define _GEN_GET     2       /* Receive file from remote system           */
#define _GEN_DATA    3       /* Data packet for file transfer             */
#define _GEN_ERROR   4       /* Error packet from remote system           */
#define _GEN_RUN     5       /* Run a program on remote system            */
#define _GEN_CD      6       /* Change directory on remote system         */
#define _GEN_LCD     7       /* Change directory on local system          */
#define _GEN_MPUT    8       /* Send multiple files to remote system      */
#define _GEN_MGET    9       /* Receive multiple files from remote system */
#define _GEN_ACK     10      /* ACKnowledge transmission                  */
#define _GEN_DONE    11      /* Terminate the conversation                */
#define _GEN_EOFTX   12      /* End of file transmission                  */
#define _GEN_CRPART  13      /* Create temporary partition                */
#define _GEN_DELPART 14      /* Delete temporary partition                */
#define _GEN_EXT     15      /* Use extensions during transfer            */
#define _GEN_LOGIN   16      /* Login information                         */
#define _GEN_HRTBEAT 17      /* Heartbeat                                 */

#define PUT          _GEN_PUT
#define GET          _GEN_GET
#define DATA         _GEN_DATA
#define ERROR        _GEN_ERROR
#define RUN          _GEN_RUN
#define CD           _GEN_CD
#define LCD          _GEN_LCD
#define MPUT         _GEN_MPUT
#define MGET         _GEN_MGET
#define ACK          _GEN_ACK
#define DONE         _GEN_DONE
#define EOFTX        _GEN_EOFTX
#define CRPART       _GEN_CRPART
#define DELPART      _GEN_DELPART
#define EXT          _GEN_EXT

/* tokens for do_genserver_commands */
#define GEN_SEND_CD		0x0001
#define GEN_USER_PART	0x0002
#define GEN_SEND_FILE	0x0004
#define GEN_SEND_MFILE	0x0008
#define GEN_RECV_FILE	0x0010
#define GEN_RECV_MFILE	0x0020
#define GEN_SEND_RUN		0x0040

/* error codes */
#define EFILENAMELENGTH 1000
#define EOWNERLENGTH    1001
#define EGROUPLENGTH    1002
#define ESEND           1003
#define ERECEIVE        1004
#define EFILEEXIST      1005
#define EDIREXIST       1007

#define OPTION_ON       1
#define OPTION_OFF      2

typedef struct PGPACKET {
    short opcode;
    char filename[FILENAMELENGTH];
    char eos1;
    mode_t permissions;
    char owner[OWNERLENGTH];
    char eos2;
    char group[GROUPLENGTH];
    char eos3;
} PGPACKET, *LPPGPACKET;

typedef struct DATAPACKET {
    short opcode;
    char buffer[1022];
} DATAPACKET, *LPDATAPACKET;

typedef struct NEWDATAPACKET {
    short opcode;
	 short bytes;
    char buffer[1022];
} NEWDATAPACKET, *LPNEWDATAPACKET;

typedef struct OLDDATAPACKET {
    short opcode;
    char buffer[1022];
} OLDDATAPACKET, *LPOLDDATAPACKET;

typedef struct ERRORPACKET {
    short opcode;
    char status[STATUSLENGTH];
    int gerrno;
    char msg[256];
    char eos;
} ERRORPACKET, *LPERRORPACKET;

typedef struct RUNPACKET {
    short opcode;
    short redirect;
} RUNPACKET, *LPRUNPACKET;

typedef struct CDPACKET {
    short opcode;
    char dirname[FILENAMELENGTH];
    char eos;
} CDPACKET, *LPCDPACKET;

typedef struct ACKPACKET {
    short opcode;
    short acking;
} ACKPACKET, *LPACKPACKET;

typedef struct DONEPACKET {
    short opcode;
} DONEPACKET, *LPDONEPACKET;

typedef struct PARTPACKET {
    short opcode;
    char func[HOSTAREALENGTH];
    char eos;
} PARTPACKET, *LPPARTPACKET;

typedef struct EXTPACKET {
   short opcode;
} EXTPACKET, *LPEXTPACKET;

typedef struct PWDPACKET {
	short opcode;
	char username[USERNAMELENGTH];
	char password[PASSWORDLENGTH];
} PWDPACKET, *LPPWDPACKET;

typedef struct HRTPACKET {
	short opcode;
} HRTPACKET, *LPHRTPACKET;

#if !defined(_WIN32)
int recvpacket(wesco_socket_t s, wesco_void_ptr_t buffer, short buffsize);
int sendpacket(wesco_socket_t s, wesco_void_ptr_t buffer, short buffsize);
int sendpg(wesco_socket_t s, short opcode, char * filename, mode_t perms, char * user, 
    char * group);
int putfile(wesco_socket_t s, char * filename, mode_t perms, char * user, char * group);
int getfile(wesco_socket_t s, char * filename, mode_t perms, char * user, char * group);
int sendcd(wesco_socket_t s, char * directory);
int wescocd(char * directory);
int cdup(wesco_socket_t s);
int sendrun(wesco_socket_t s, short redirect, LPGENTRANSAREA parms);
int getparms(wesco_socket_t s, LPGENTRANSAREA parms);
int mget(wesco_socket_t s);
int mput(wesco_socket_t s, char * pattern);
int sendack(wesco_socket_t s, short opcode);
int RemoteFiletoPG(LPPGPACKET pg, LPREMOTEFILE rf);
int setgrown(char * filename, char * owner, char * group);
int getinfo(int fd, mode_t *mode, char * owner, char * group);
int sendpp(wesco_socket_t s, short opcode, char * func);
int sendext(wesco_socket_t s);
int senderror(wesco_socket_t s, char * lpStatus, char * lpMsg);
#endif      /* !defined(_WIN32) */

#ifdef GENLIB
int generr;
char *genmsg[] = {
    "Filename is too long.",
    "User length is too long.",
    "Group length is too long.",
    "Error sending file.",
    "Error receiving file.",
    "File does not exist.",
    "Directory does not exist."
};

char *commands[] = { "put", "get", "run", "cd", "lcd", "open", "close",
                     "bye", "mget", "mput", "cdup", "crpart", "delpart" };

#else
extern int generr;
extern char *genmsg[];
extern char *commands[];
#endif

#endif
