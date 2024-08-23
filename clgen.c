#if defined(__hpux)
#   define _POSIX_SOURCE
#   define _XOPEN_SOURCE
#   define _XOPEN_SOURCE_EXTENDED 1
#endif

#ifdef unix
#define UNIXCLIENT
#endif

#define WESCOMCLIENT
#define RDIR

#include <sys/types.h>
#include <sys/socket.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/ttold.h>
#endif

#include <sys/utsname.h>
#include <netinet/in.h>
#include <netdb.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#include <termios.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "more.h"
#include "clgen.h"
#include "setsignal.h"

bool_t isacked(int);

char *clgen;
extern char *optarg;
bool_t debugsocket = FALSE;
int  debuglog = 0;
bool_t interactive = FALSE;
bool_t debugFile = FALSE;
bool_t connected = FALSE;
char service[12];
char protocol[4];
char version[VERSIONLENGTH+1];
char filename[FILENAMELENGTH+1];
char branchname[HOSTNAMELENGTH+1];
GENTRANSAREA parms;

char *devnull = "/dev/null";
int s = 0;

int main(int argc, char *argv[])
{
   int option;
   int tempfd;
   int retval = 0;
   mode_t oldmask;
   struct utsname brinfo;
   bool_t usefile = TRUE;
   struct sigaction alarmact;
   extern int optind;

   if (argc < 2) {
      err_msg("invalid parameter count\n");
      exit(1);
   }

   /* process the command line options */
   while ((option = getopt(argc, argv, "?dp:s")) != EOF) {
      switch (option) {
         case '?':   /* begging for help */
            exit(0);
            break;
         case 'd':   /* write to debug log */
            debuglog = creat("clgen.dbg", READWRITE);
            if (debuglog < 0) {
               err_ret("error creating log file");
               debuglog = 0;
            }
            else
               wescolog(debuglog, "log created\n");

            break;
         case 'p':   /* use following parameters */
            setparameters(optarg);
            usefile = FALSE;
            break;
         case 's':   /* socket-level debugging */
            debugsocket = TRUE;
            break;
         default:    /* unknown options */
            exit(1);
      }
   }

   /* set the file mode creation mask */
   oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

	setsignal(SIGHUP, SIG_IGN, NULL, 0, 0);
	setsignal(SIGPIPE, SIG_IGN, NULL, 0, 0);

   /* get the network name */
   uname(&brinfo);
   if (toupper(brinfo.nodename[0]) == 'B')
      strcpy(branchname, &brinfo.nodename[1]);
   else
      strcpy(branchname, brinfo.nodename);

   /* the remaining parameter may be a file name */
   if (argv[optind] != 0)
      strcpy(filename, argv[optind]);

   retval = callclient(usefile);

   if (debuglog)
      close(debuglog);

   /* restore file creation mask */
   umask(oldmask);

   return retval;
}

int callclient(bool_t usefile)
{
  int fd;                         /* file descriptor                  */
  char status[STATUSLENGTH+1];    /* status returned from WDOCGENERIC */

  if (debuglog)
    wescolog(debuglog, "calling WDOCGENERIC\n");

  if (usefile) {
     /* read transaction area from file */
    if ((fd = open(filename, O_RDONLY)) < 0)
      err_sys("error opening %s\n", filename);

    if (readn(fd, &parms, sizeof(parms)) < 0)
      err_sys("error reading %s\n", filename);

    close(fd);
  }

  WDOCGENERIC(&parms);

  if (debuglog)
    wescolog(debuglog, "returned from WDOCGENERIC\n");

   /* get the return status */
  CobolToC(status, parms.status, sizeof(status), sizeof(parms.status));

  if (debuglog)
    wescolog(debuglog, "return status = %s\n", status);

  return atoi(status);
}

int setparameters(char *args)
{
   char *options;    /* pointer to the suboptions   */
   char *value;      /* value of the field          */
   int retvalue;     /* index into suboptions array */

   options = args;
   while (*options != 0) {
      retvalue = getsubopt(&options, suboptions, &value);
      if (value == NULL)
         err_quit("%s - missing value\n", suboptions[retvalue]);

      switch (retvalue) {
         case GTA_TRANS_TYPE:
            CtoCobol(parms.transaction, value, sizeof(parms.transaction));
            if (debuglog)
               wescolog(debuglog, "GTA-TRANS-TYPE = %s\n", value);
            break;
         case GTA_HOST_NAME:
            CtoCobol(parms.hostname, value, sizeof(parms.hostname));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-NAME = %s\n", value);
            break;
         case GTA_CLIENT_NAME:
            CtoCobol(parms.clientname, value, sizeof(parms.clientname));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-NAME = %s\n", value);
            break;
         case GTA_OPERATION_MODE:
            CtoCobol(parms.opmode, value, sizeof(parms.opmode));
            if (debuglog)
               wescolog(debuglog, "GTA-OPERATION-MODE = %s\n", value);
            break;
         case GTA_USER_NAME:
            CtoCobol(parms.username, value, sizeof(parms.username));
            if (debuglog)
               wescolog(debuglog, "GTA-USER-NAME = %s\n", value);
            break;
         case GTA_HOST_AREA:
            CtoCobol(parms.hostarea, value, sizeof(parms.hostarea));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-AREA = %s\n", value);
            break;
         case GTA_HOST_PROCESS:
            CtoCobol(parms.hostprocess, value, sizeof(parms.hostprocess));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-PROCESS = %s\n", value);
            break;
         case GTA_CLIENT_SND_FILE_NAME:
            CtoCobol(parms.clientsendfile,value,sizeof(parms.clientsendfile));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-SND-FILE-NAME = %s\n", value);
            break;
         case GTA_HOST_RCV_FILE_NAME:
            CtoCobol(parms.hostrecvfile.name, value,
               sizeof(parms.hostrecvfile.name));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-RCV-FILE-NAME = %s\n", value);
            break;
         case GTA_HOST_RCV_FILE_PERM:
            CtoCobol(parms.hostrecvfile.permissions, value,
               sizeof(parms.hostrecvfile.permissions));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-RCV-FILE-PERM = %s\n", value);
            break;
         case GTA_HOST_RCV_FILE_OWNER:
            CtoCobol(parms.hostrecvfile.owner, value,
               sizeof(parms.hostrecvfile.owner));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-RCV-FILE-OWNER = %s\n", value);
            break;
         case GTA_HOST_RCV_FILE_GROUP:
            CtoCobol(parms.hostrecvfile.group, value,
               sizeof(parms.hostrecvfile.group));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-RCV-FILE-GROUP = %s\n", value);
            break;
         case GTA_HOST_SND_FILE_NAME:
            CtoCobol(parms.hostsendfile, value, sizeof(parms.hostsendfile));
            if (debuglog)
               wescolog(debuglog, "GTA-HOST-SND-FILE-NAME = %s\n", value);
            break;
         case GTA_CLIENT_RCV_FILE_NAME:
            CtoCobol(parms.clientrecvfile.name, value,
               sizeof(parms.clientrecvfile.name));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-RCV-FILE-NAME = %s\n", value);
            break;
         case GTA_CLIENT_RCV_FILE_PERM:
            CtoCobol(parms.clientrecvfile.permissions, value,
               sizeof(parms.clientrecvfile.permissions));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-RCV-FILE-PERM = %s\n", value);
            break;
         case GTA_CLIENT_RCV_FILE_OWNER:
            CtoCobol(parms.clientrecvfile.owner, value,
               sizeof(parms.clientrecvfile.owner));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-RCV-FILE-OWNER = %s\n", value);
            break;
         case GTA_CLIENT_RCV_FILE_GROUP:
            CtoCobol(parms.clientrecvfile.group, value,
               sizeof(parms.clientrecvfile.group));
            if (debuglog)
               wescolog(debuglog, "GTA-CLIENT-RCV-FILE-GROUP = %s\n", value);
            break;
         case GTA_IO_FLAG:
            parms.redirect = *value;
            if (debuglog)
               wescolog(debuglog, "GTA-IO-FLAG = %s\n", value);
            break;
         case GTA_SPECIAL_SWITCHES:
            CtoCobol(parms.switches, value, sizeof(parms.switches));
            if (debuglog)
               wescolog(debuglog, "GTA-SPECIAL-SWITCHES = %s\n", value);
            break;
         case GTA_DETAIL:
            CtoCobol(parms.details, value, sizeof(parms.details));
            if (debuglog)
               wescolog(debuglog, "GTA-DETAIL = %s\n", value);
            break;
         default:
            err_quit("unknown field - \"%s\"\n", value);
      }
   }

   return 0;
}
