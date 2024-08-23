#ifndef __INVSERVERH__
#define __INVSERVERH__

#define MAXRECEIVES             50
#define MAXRECORDS              73
#define NUMVERSIONSUPPORTED     5
#define DEBUGSERVER
#define DEBUG

#define V100300RECORDLENGTH 69
#define V100600RECORDLENGTH 96
#define V100901RECORDLENGTH 104

typedef struct {
	char status[STATUSLENGTH];
	ERRORSTR error;
	char transaction[TRANSACTIONSIZE];
	char branch[BRANCHLENGTH];
	char sku[SKULENGTH];
	char counter[COUNTERLENGTH];
	char results[MAXRECORDS][RECORDLENGTH];
	char filename[FILENAMELENGTH];
} COBOLPARMS;

typedef struct {
	char status[STATUSLENGTH];
	ERRORSTR error;
	char transaction[TRANSACTIONSIZE];
	char branch[BRANCHLENGTH];
	char sku[SKULENGTH];
	char counter[COUNTERLENGTH];
	char results[MAXRECORDS][V100300RECORDLENGTH];
	char filename[FILENAMELENGTH];
   char cust[CUSTOMERLENGTH];
} COBOLPARMS2;

typedef struct {
	char status[STATUSLENGTH];
	ERRORSTR error;
	char transaction[TRANSACTIONSIZE];
	char branch[BRANCHLENGTH];
	char sku[SKULENGTH];
	char counter[COUNTERLENGTH];
	char results[MAXRECORDS][V100600RECORDLENGTH];
	char filename[FILENAMELENGTH];
   char cust[CUSTOMERLENGTH];
} COBOLPARMS3;

typedef struct {
	char status[STATUSLENGTH];
	ERRORSTR error;
	char transaction[TRANSACTIONSIZE];
	char branch[BRANCHLENGTH];
	char sku[SKULENGTH];
	char counter[COUNTERLENGTH];
	char results[MAXRECORDS][V100901RECORDLENGTH];
	char filename[FILENAMELENGTH];
    char cust[CUSTOMERLENGTH];
} COBOLPARMS4;

int s, ls;

struct hostent *hp;
struct servent *sp;

struct linger linger;
struct sockaddr_in myaddr_in, peeraddr_in;

bool_t debugFile;
bool_t debugSocket;
int db;
char debugfilename[FILENAMELENGTH];

void AcceptConnections(void);
void Server060400(char *, char *, int);
void Server060503(char *, char *, int);
void Server100300(char *, char *, int);
void Server100600(char *, char *, int);
void Server100901(char *, char *, int);

char service[25];
char protocol[25];
char *progName = 0;

#ifndef TESTSERVER
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
    { "10.09.01", Server100901, "GETXSIN", 512},
    { "10.06.00", Server100600, "GETCSCIN", 512 },
    { "10.03.00", Server100300, "GETINVBI", 512 },
    { "06.05.03", Server060503, "GETINVTC", 512 },
    { "060402dc", Server060400, "GETINVTC", 512 }
};
#else
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
    { "10.09.01", Server100901, "GETXSIN", 512},
    { "10.06.00", Server100600, "GETCSCIN", 512 },
    { "10.03.00", Server100300, "GETINVBI", 512 },
    { "06.05.03", Server060503, "GETINVTC", 512 },
    { "060402dc", Server060404, "GETINVTC", 512 }
};
#endif

extern short A_call_err;

#endif
