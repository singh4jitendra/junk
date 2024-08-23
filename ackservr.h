#ifndef __ACKSERVERH__
#define __ACKSERVERH__

#define MAXRECEIVES             5
#define MAXRECORDS              25
#define NUMVERSIONSUPPORTED     2
#define NETWORKLOG              (-1)

typedef struct {
	char transaction[TRANSACTIONSIZE];
	char hostname[HOSTNAMELENGTH];
	char username[USERNAMELENGTH];
	char filename[FILENAMELENGTH];
	char partition[PARTITIONLENGTH];
	char status[STATUSLENGTH];
	ERRORSTR error;
	char branch[BRANCHLENGTH];
	char sku[SKULENGTH];
	char mode[MODELENGTH];
	char counter[COUNTERLENGTH];
	char results[MAXRECORDS][PORECORDLENGTH];
} RPOACKPARMS;

int s, ls;

struct hostent *hp;
struct servent *sp;

struct linger linger;
struct sockaddr_in myaddr_in, peeraddr_in;

bool_t debugCobol;
bool_t debugFile;
bool_t debugSocket;
int db;
char service[25];
char protocol[25];
char debugfilename[FILENAMELENGTH];
char *progName = 0;

void AcceptConnections(void);
void Server060400(char *, char *, int);
void Server060503(char *, char *, int);
void ParseArguments(int argc, char *argv[]);

#ifndef TESTSERVER
SERVERVERSIONCONTROL ackversionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.03", Server060503, "RPOACK", 512 },
  { "06.04.00", Server060400, "RPOACK", 512 }
};
#else
SERVERVERSIONCONTROL ackversionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.03", Server060503, "TRPOACK", 512 },
  { "06.04.00", Server060400, "TRPOACK", 512 }
};
#endif

extern short A_call_err;

#endif
