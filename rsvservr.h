#ifndef __RSVSERVERH__
#define __RSVSERVERH__

/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Added new version for bonded inventory.
 ******************************************************************************
*/
#define MAXRECEIVES     50
#define NUMVERSIONSUPPORTED 3

#define NETWORKLOG    (-1)

int s, ls;

struct hostent *hp;
struct servent *sp;

struct linger linger;
struct sockaddr_in myaddr_in, peeraddr_in;

bool_t debugCobol;
bool_t debugFile;
bool_t debugSocket;
extern short A_call_err;
int db;

void AcceptConnections(void);
void Server060500(char *, char *, int);
void Server060400(char *, char *, int);
void Cleanup(void);

char debugfilename[FILENAMELENGTH];
char service[25];
char protocol[25];

#ifndef TESTSERVER
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
 { "10.03.00", Server060500, "REMRSVBI", 512 },
 { "06.05.00", Server060500, "REMRSV", 512 },
 { "06.04.00", Server060400, "REMRSV", 512 }
};
#else
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
 { "10.03.00", Server060500, "REMRSVBI", 512 },
 { "06.05.00", Server060500, "TREMRSV", 512 },
 { "06.04.00", Server060400, "TREMRSV", 512 }
};
#endif

typedef FUNCPARMS COBOLPARMS;

#endif
