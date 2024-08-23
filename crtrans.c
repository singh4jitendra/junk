#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

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
#define SWITCHLENGTH            12

typedef struct {
    char name[FILENAMELENGTH];
    char permissions[PERMISSIONSLENGTH];
    char owner[OWNERLENGTH];
    char group[GROUPLENGTH];
} REMOTEFILE;

typedef struct {
    char name[ERRORNAMELENGTH];
    char description[ERRORDESCLENGTH];
} ERRORSTR;

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
} GENTRANSAREA;

#define GTAPARAMETER(x,y) strncpy(x, y, strlen(y));

int main(int argc, char * argv[])
{
	int fd;
	int fillchar;
	GENTRANSAREA gta;

	if (argc >= 2)
		fillchar = argv[1][0];
	else
		fillchar = 0;

	memset(&gta, fillchar, sizeof(gta));

	GTAPARAMETER(gta.transaction, "SHELL")
	GTAPARAMETER(gta.hostname, "QA13")
	GTAPARAMETER(gta.clientname, "QA?")
	GTAPARAMETER(gta.opmode, "11")
	GTAPARAMETER(gta.username, "TEST")
	GTAPARAMETER(gta.status, "00")
	GTAPARAMETER(gta.hostarea, "TEST")
	GTAPARAMETER(gta.hostprocess, "ls -l > ls.out")
	GTAPARAMETER(gta.clientsendfile, "junkfile")
	GTAPARAMETER(gta.hostsendfile, "ls.out")
	GTAPARAMETER(&(gta.redirect), "1")
	GTAPARAMETER(gta.switches, "-d")

	fd = creat("junkfile", 0666);
	write(fd, &gta, sizeof(gta));
	close(fd);

	return 0;
}
