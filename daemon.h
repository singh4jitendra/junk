#ifndef _DAEMON_H_
#define _DAEMON_H_

#ifndef _BDB_H_
#include "bdb.h"
#endif

#define SINGLEPROCESSED     1
#define CONCURRENTPROCESSED 2
#define MULTIPROCESSED      3

#define PERMS               0666

typedef struct sockaddr_in SOCKADDR;
typedef struct servent SERVENT;
typedef struct hostent HOSTENT;

char logfile[256];
char service[20];
char protocol[4];
char servername[256];
char keyname[256];
char *progName;
int servertype;
int numProcesses;
int numBranches;
int maxListen;
int socketdebug;
int maxBranches;
int debugFile;
int debugCobol;
int db;
int s;
int ls;
int btId;
int ptId;
int timeout;
int myIndex;
int branchadd;
int processadd;

BRANCHTABLE *branchTable;
PROCESSTABLE *processTable;
SOCKADDR myaddr;
SOCKADDR peeraddr;
SERVENT *sp;
HOSTENT *hp;

int CreateListenSocket(SOCKADDR *, int, int);
void AcceptConnections(void);
int ReadIni(char *);
int ReadGeneral(FILE *);
int ReadCommunications(FILE *);
int ReadBranches(FILE *);
int ForkProcesses(int);
int GetBranchIndex(char *);
void HandOffSocket(int, char *);
int GetProcessIndex(pid_t);
int ParseDaemonArguments(int, char *[]);
void Cleanup(void);

#endif
