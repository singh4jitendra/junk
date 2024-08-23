#ifndef __MAINTSERVERH__
#define __MAINTSERVERH__

#define MAXRECEIVES     50
#define NUMVERSIONSUPPORTED 2

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
void Server060400(char *, char *, int);
void Server060500(char *, char *, int);
void Cleanup(void);
 
#ifndef TESTSERVER
char *service = "WDOCUPDCP";
char *protocol = "tcp";
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.00", Server060500, "UPDINVCP", 512 },
  { "06.04.00", Server060400, "UPDINV", 512 }
};
#else
char *service = "TWDOCUPDCP";
char *protocol = "tcp";
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.00", Server060500, "TUPDINVCP", 512 },
  { "06.04.00", Server060400, "TUPDINV", 512 }
};
#endif

#endif
