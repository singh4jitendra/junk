#ifndef _PROCESS_H_
#define _PROCESS_H_

#define NUMVERSIONSUPPORTED 2
#define PERMS 0666

#ifndef _BDB_H_
#include "bdb.h"
#endif

void Server060500(char *, char *, int);
void Server060400(char *, char *, int);
void Cleanup(void);

#ifdef MAINMODULE
/* Variable definitions for the main module */
char servername[256];
char keyname[256];
char *progName;
int servertype;
int numProcesses;
int maxListen;
int socketdebug;
int processsize;
int branchsize;
int processkey;
int branchkey;
int db;
int s;
int btId;
int ptId;
int timeout;
int myindex;
int maxBranches;
int numBranches;
int *ips;
char networkErrorMessage[256];
int mypipe;

BRANCHTABLE *branchTable;
PROCESSTABLE *processTable;

#ifndef TESTSERVER
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.00", Server060500, "UPDINVCP", 512 },
  { "06.04.00", Server060400, "UPDINV", 512 }
};
#else
char *progName = 0;
SERVERVERSIONCONTROL versionTable[NUMVERSIONSUPPORTED] = {
  { "06.05.00", Server060500, "TUPDINVCP", 512 },
  { "06.04.00", Server060400, "TUPDINV", 512 }
};
#endif
#else
/* This is a server function module */
extern int db;
extern int s;
extern int myindex;
extern char *progName;
extern char networkErrorMessage[256];
extern BRANCHTABLE *branchTable;
extern PROCESSTABLE *processTable;
#endif

extern int debugCobol;
extern int debugFile;
extern int errno;
extern int A_call_err;
#endif
