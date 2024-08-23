#ifndef _GENSERVER_H_
#define _GENSERVER_H_

#define MAXRECEIVES     50
#define NUMVERSIONSUPPORTED 5

#define SHM_GENSERVER (('W' << 16) + 44)
#define SEM_GENSERVER (('W' << 16) + 44)
#define SEM_EXIT 10

#define GS_SET_MAX_CONNECTIONS   0x0001
#define GS_RESET_NUM_CONNECTIONS 0x0002
#define GS_VIEW_SHARED_MEMORY    0x0004
#define GS_SET_NUM_CONNECTIONS   0x0010

int s, ls;

struct hostent *hp;

struct linger linger;
struct sockaddr_in myaddr_in, peeraddr_in;

bool_t debugCobol;
bool_t debugFile;
extern short A_call_err;
int db;
char service[25];
char protocol[25];

char *cobolFunc = "TRAFFICOP";
char *progName = 0;

void AcceptConnections(void);
void Server080501(char *, char *, int, int);
void Server080500(char *, char *, int, int);
void Server060503(char *, char *, int, int);
void Cleanup(void);

typedef struct gendata {
	int32_t numconnections;
	int32_t maxconnections;
} gendata_t;

typedef struct GENSERVERVERSIONCONTROL {
	char number[VERSIONLENGTH];
	void (* Server)(char *, char *, int, int);
	char cobolProgram[FILENAMELENGTH];
	bool_t requires_login;
	bool_t heartbeat;
} GENSERVERVERSIONCONTROL;

GENSERVERVERSIONCONTROL srvrVersionTable[NUMVERSIONSUPPORTED] = {
  { "\x00\x08\x00\x08\x00\x01UNX", Server080500, "TRAFFICOP", FALSE, TRUE },
  { "\x00\x08\x00\x05\x00\x01WIN", Server080501, "TRAFFICOP", TRUE, FALSE },
  { "\x00\x08\x00\x05\x00\x00WIN", Server080500, "TRAFFICOP", TRUE, FALSE },
  { "\x00\x08\x00\x05\x00\x00UNX", Server080500, "TRAFFICOP", FALSE, FALSE },
  { "06.05.03", Server060503, "TRAFFICOP", FALSE, FALSE }
};

#endif
