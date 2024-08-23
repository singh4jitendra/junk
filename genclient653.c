#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/ttold.h>
#endif


#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE) && !defined(_GNU_SOURCE)
#   include <sys/sockio.h>
#endif    /* _HPUX_SOURCE */

#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#include <termios.h>
#endif

#include <string.h>
#include <libgen.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>

#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#include "common.h"
#include "receive.h"
#include "generic.h"
#include "gen2.h"
#include "more.h"
#include "readini.h"

char cookey[] = "\xff\xf0\xfc\xfd";
int defflags;

#define GETACK(x, y)      if (!acked(x, y)) { \
                              close(x); \
                              return -1; \
                          }

int clientget(int, GENTRANSAREA *);
int clientput(int, GENTRANSAREA *);
int clientrun(int, GENTRANSAREA *);
int clientmget(int, GENTRANSAREA *);
int clientmput(int, GENTRANSAREA *);
int acked(int, GENTRANSAREA *);
void childoutofband(int);
void parentoutofband(int);
void funeral(int);
void plumber(int);
void repair(void);
int rawterminal(void);
int queuetransaction(int, bool_t, GENTRANSAREA *);
bool_t CopyFile(char *, char *);
int reader(int, int);
int sendevar(int, char *);

char gin[1024];
char gout[1024];

char *evar_list[] = {
   "HICOLOR",
   "TERM",
   "NOVATERM",
   ""
};

#define lendianswap(x) (x)
#define endianswap(x) (x)

struct termios cliterm;
struct termios saveterm;
struct winsize cliwin;
pid_t childpid;
static volatile sig_atomic_t netdone;
static volatile int deadchild;
static volatile int brokenpipe;
GENTRANSAREA oobparms;

int clientput(int s, GENTRANSAREA *parms)
{
   char filename[FILENAMELENGTH + 1];
   PGPACKET *pg;
   char *owner;
   char *group;
   char *filebase;
   char *temp;
   char qprefix[FILENAMELENGTH];
   char truename[FILENAMELENGTH];
   u_int32_t trash1, trash2;

   GetPrivateProfileString("genserver", "queueprefix", qprefix,
      sizeof(qprefix), "genqueue", WESCOMINI);

   pg = (PGPACKET *)gout;

   memset(pg, 0, sizeof(PGPACKET));

   RemoteFiletoPG(pg, &(parms->hostrecvfile));

   if (pg->filename[0] <= 32) {
      CobolToC(filename, parms->clientsendfile, sizeof(filename),
         sizeof(parms->clientsendfile));
      filebase = strdup(filename);
      /* check to see if the filename starts with the queue prefix */
      temp = basename(filebase);
      if (!strncmp(temp, qprefix, strlen(qprefix))) {
         sscanf(temp+strlen(qprefix)+1, "%lu.%lu.%s", &trash1, &trash2,
            truename);
      }
      else
         strcpy(truename, temp);

      strcpy(pg->filename, truename);
      free(filebase);
   }
   else {
      strcpy(filename, pg->filename);
      memset(pg->filename, 0, sizeof(pg->filename));
      strcpy(pg->filename, basename(filename));
   }

   owner = (pg->owner[0] == 0) ? NULL : pg->owner;
   group = (pg->group[0] == 0) ? NULL : pg->group;

   if (sendpg(s, PUT, pg->filename, pg->permissions, owner, group) < 0) {
      generr = ESEND;
      return -1;
   }

   CobolToC(filename, parms->clientsendfile, sizeof(filename), 
      sizeof(parms->clientsendfile));
   if (putfile(s, filename, 0, NULL, NULL) < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int clientget(int s, GENTRANSAREA *parms)
{
   PGPACKET *pg;
   char *ptr_owner;
   char *ptr_group;
   char filename[FILENAMELENGTH + 1];

   pg = (PGPACKET *)gout;

   memset(pg, 0, sizeof(PGPACKET));

   RemoteFiletoPG(pg, &(parms->clientrecvfile));

   CobolToC(filename, parms->hostsendfile, sizeof(filename), 
      sizeof(parms->hostsendfile));
   if (sendpg(s, GET, filename, pg->permissions, pg->owner, pg->group) < 0) {
      generr = ESEND;
      return -1;
   }

   ptr_owner = (pg->owner[0] <= 32) ? NULL : pg->owner;
   ptr_group = (pg->group[0] <= 32) ? NULL : pg->group;

   if (getfile(s, pg->filename, pg->permissions, ptr_owner, ptr_group) < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int clientrun(int s, GENTRANSAREA *parms)
{
   short redirect;
   short count;
   int ios;
   unsigned short sport = IPPORT_RESERVED;
   int cs;
   struct sockaddr_in pa;
   SOCKLEN_T size;

   switch (parms->redirect) {
      case '1':
         redirect = TRUE;
         break;
      default:
         redirect = FALSE;
         break;
   }

   if (sendrun(s, redirect, parms) < 0) {
      generr = ESEND;
      return -1;
   }

   if (redirect) {
      redirect ^= 0x8000;

      if ((cs = rsvport(&sport)) < 0)
         return -1;

      tcgetattr(STDOUT_FILENO, &cliterm);
      if (sendpacket(s, &cliterm, sizeof(cliterm)) < 0)
         return -1;

      ioctl(STDOUT_FILENO, TIOCGWINSZ, &cliwin);
      if (sendpacket(s, &cliwin, sizeof(cliwin)) < 0)
         return -1;

      count = -1;
      do {
         count++;
         sendevar(s, evar_list[count]);
      } while (strcmp(evar_list[count], ""));

      if (sendpacket(s, &sport, sizeof(sport)) < 0)
         return -1;

      listen(cs, 1);
      size = (SOCKLEN_T)sizeof(pa);
      if ((ios = accept(cs, (struct sockaddr *)&pa, &size)) < 0) {
         close(cs);
         return -1;
      }

      close(cs);

      signal(SIGPIPE, plumber);
      setsocketkeepalive(ios, 1);
      setsocketkeepalive(s, 1);
      rawterminal();
      reader(ios, s);
      tcsetattr(STDOUT_FILENO, TCSANOW, &cliterm);
      setsocketkeepalive(s, 0);
      close(ios);
   }

   if (getparms(s, parms) < 0) {
      generr = ERECEIVE;
      return -1;
   }

   return 0;
}

int clientmput(int s, GENTRANSAREA *parms)
{
   char filename[FILENAMELENGTH + 1];
   PGPACKET *pg;
   char *owner;
   char *group;

   pg = (PGPACKET *)gout;

   memset(pg, 0, sizeof(PGPACKET));

   RemoteFiletoPG(pg, &(parms->hostrecvfile));

   if (pg->filename[0] <= 32) {
      CobolToC(pg->filename, parms->clientsendfile, sizeof(pg->filename),
         sizeof(parms->clientsendfile));
   }

   owner = (pg->owner[0] == 0) ? NULL : pg->owner;
   group = (pg->group[0] == 0) ? NULL : pg->group;

   if (sendpg(s, MPUT, pg->filename, pg->permissions, owner, group) < 0) {
      generr = ESEND;
      return -1;
   }

   CobolToC(filename, parms->clientsendfile, sizeof(filename), 
      sizeof(parms->clientsendfile));
   if (mput(s, filename) < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int clientmget(int s, GENTRANSAREA *parms)
{
   PGPACKET *pg;
   char filename[FILENAMELENGTH + 1];

   pg = (PGPACKET *)gout;

   memset(pg, 0, sizeof(PGPACKET));

   RemoteFiletoPG(pg, &(parms->clientrecvfile));

   CobolToC(filename, parms->hostsendfile, sizeof(filename), 
      sizeof(parms->hostsendfile));
   if (sendpg(s, MGET, filename, pg->permissions, pg->owner, pg->group) < 0) {
      generr = ESEND;
      return -1;
   }

   return ((mget(s) < 0) ? -1 : 0);
}

int acked(int s, GENTRANSAREA *parms)
{
   char buffer[270];
   short *opcode;
   ERRORPACKET *err;

   if (recvpacket(s, buffer, sizeof(buffer)) < 0) {
		strncpy(parms->status, "03", 2);
      return FALSE;
	}

   opcode = (short *)buffer;
   if (*opcode == ERROR) {
      err = (ERRORPACKET *)buffer;
      strncpy(parms->status, err->status, 2);
      strncpy(parms->error.description, err->msg, strlen(err->msg));

      return FALSE;
   }

   return TRUE;
}

/* ARGSUSED */
int GenClient060503(wesco_socket_t s, int32_t * lpGenFlags, LPGENTRANSAREA lpGtaParms, int heartbeat)
{
	DONEPACKET dpDone = { _GEN_DONE };

	if (*lpGenFlags & GEN_SEND_CD) {
		CobolToC(gout, lpGtaParms->hostarea, sizeof(gout),
			sizeof(lpGtaParms->hostarea));
		if (sendcd(s, gout) < 0) {
			CtoCobol(lpGtaParms->status, "02", sizeof(lpGtaParms->status));
			return -1;
		}

		GETACK(s, lpGtaParms);
	}
	else if (*lpGenFlags & GEN_USER_PART) {
		CobolToC(gout, lpGtaParms->hostarea, sizeof(gout),
			sizeof(lpGtaParms->hostarea));
		if (sendpp(s, CRPART, gout) < 0) {
			CtoCobol(lpGtaParms->status, "11", sizeof(lpGtaParms->status));
			return -1;
		}

		GETACK(s, lpGtaParms);
	}

	if (*lpGenFlags & GEN_SEND_FILE) {
		if (clientput(s, lpGtaParms) < 0) {
			CtoCobol(lpGtaParms->status, "16", sizeof(lpGtaParms->status));
			return -1;
		}

		GETACK(s, lpGtaParms);
	}
	else if (*lpGenFlags & GEN_SEND_MFILE) {
		if (clientmput(s, lpGtaParms) < 0) {
			CtoCobol(lpGtaParms->status, "16", sizeof(lpGtaParms->status));
			return -1;
		}

		GETACK(s, lpGtaParms);
	}

	if (*lpGenFlags & GEN_SEND_RUN) {
		if (clientrun(s, lpGtaParms) < 0) {
			CtoCobol(lpGtaParms->status, "05", sizeof(lpGtaParms->status));
			return -1;
		}

		GETACK(s, lpGtaParms);
	}

	if (*lpGenFlags & GEN_RECV_FILE) {
		if (clientget(s, lpGtaParms) < 0) {
			return -1;
		}

		GETACK(s, lpGtaParms);
	}
	else if (*lpGenFlags & GEN_RECV_MFILE) {
		if (clientmget(s, lpGtaParms) < 0) {
			return -1;
		}

		GETACK(s, lpGtaParms);
	}

	if (*lpGenFlags & GEN_USER_PART) {
		if (sendpp(s, DELPART, NULL) < 0) {
			return -1;
		}

		GETACK(s, lpGtaParms);
	}

	sendpacket(s, &dpDone, sizeof(dpDone));

	return 0;
#if 0
   char opModeStr[OPMODELENGTH+1];
   int  opMode;
   short done = DONE;
   int ios;

   CobolToC(opModeStr, parameters->opmode, sizeof(opModeStr),
      sizeof(parameters->opmode));
   sscanf(opModeStr, "%02d", &opMode);

   tcgetattr(STDOUT_FILENO, &saveterm);

   switch (opMode) {
      case SPECIFIEDPART_TRANS:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_INFILE:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_OUTFILE:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_IOFILES:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_MULTIN:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         memset(gout, 0, sizeof(gout));
         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_MULTOUT:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case SPECIFIEDPART_MULTIO:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendcd(s, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         memset(gout, 0, sizeof(gout));
         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_TRANS:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_INFILE:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_OUTFILE:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_IOFILES:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_MULTIN:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_MULTOUT:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));
         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case CREATEDPART_MULTIO:
         CobolToC(gout, parameters->hostarea, sizeof(gout),
            sizeof(parameters->hostarea));

         if (sendpp(s, CRPART, gout) < 0) {
            strncpy(parameters->status, "18", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         memset(gout, 0, sizeof(gout));
         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientrun(s, parameters) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (sendpp(s, DELPART, NULL) < 0) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_SEND:
         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_RETRIEVE:
         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_SENDRETRIEVE:
         /* Send files to server */
         if (clientput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         /* Retrieve files from server */
         if (clientget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_MULTISEND:
         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_MULTIRETRIEVE:
         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;
      case FILE_MULTISENDRETRIEVE:
         CobolToC(gout, parameters->clientsendfile, sizeof(gout),
            sizeof(parameters->clientsendfile));

         if (clientmput(s, parameters) < 0) {
            strncpy(parameters->status, "16", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         if (clientmget(s, parameters) < 0) {
            strncpy(parameters->status, "17", 2);
            close(s);
            return;
         }

         GETACK(s, parameters);

         break;

      case GQ_SPECIFIEDPARTITION:
      case GQ_CREATEDPARTITION:
         queuetransaction(opMode, FALSE, parameters);
         break;
      case GQ_SPECIFIEDPART_INFILE:
      case GQ_CREATEDPART_INFILE:
      case GQ_SPECIFIEDPART_MULTIN:
      case GQ_CREATEDPART_MULTIN:
         queuetransaction(opMode, TRUE, parameters);
         break;
      default:
         strncpy(parameters->status, "18", 2);
         close(s);
         return;
   };

   sendpacket(s, &done, sizeof(short));

   close(s);
#endif
}

/* ARGSUSED */
void funeral(int trash)
{
   deadchild = TRUE;
}

int rawterminal(void)
{
   struct termios temp;

   if (tcgetattr(STDIN_FILENO, &temp) < 0)
      return -1;

   temp.c_lflag &= ~(ECHO|ICANON);
   temp.c_cc[VMIN] = 1;
   temp.c_cc[VTIME] = 0;

   return ((tcsetattr(STDIN_FILENO, TCSAFLUSH, &temp) < 0) ? -1 : 0);
}

int reader(int s, int ds)
{
   int maxfd, cc, i, atoobmark;
   register int ptycc, sockcc;
   char ptyibuf[1024], sockibuf[1024], *ptybptr, *sockbptr;
   fd_set inbits, outbits, errbits, *ob;
   char me[128];

   ptycc = sockcc = 0;

   maxfd = 1 + s;

   FD_ZERO(&inbits);
   FD_ZERO(&outbits);
   FD_ZERO(&errbits);

   for ( ; ; ) {
      if (netdone || brokenpipe || deadchild)
         return 0;

      FD_ZERO(&inbits);
      FD_ZERO(&outbits);
		FD_ZERO(&errbits);

      ob = (fd_set *)NULL;

      if (sockcc > 0) {
         FD_SET(STDOUT_FILENO, &outbits);
         ob = &outbits;
      }
      else
         FD_SET(s, &inbits);

      if (ptycc > 0) {
         FD_SET(s, &outbits);
         ob = &outbits;
      }
      else {
         FD_SET(STDIN_FILENO, &inbits);
		}

      FD_SET(s, &errbits);
      FD_SET(ds, &inbits);

      if (select(maxfd, &inbits, ob, &errbits,
            (struct timeval *)0) < 0) {
         if (errno == EINTR) {
            continue;
			}

         return -1;
      }

      if (FD_ISSET(s, &inbits)) {
         sockcc = read(s, sockibuf, sizeof(sockibuf));
         if (sockcc < 0 && errno == EWOULDBLOCK) {
            sockcc = 0;
            errno = 0;
         }
         else if (sockcc == -1)
            return -1;
      }

      if (sockcc > 0 && FD_ISSET(STDOUT_FILENO, &outbits)) {
         sockbptr = sockibuf;
         while (sockcc > 0) {
            cc = write(STDOUT_FILENO, sockbptr, sockcc);
            sockcc -= cc;
            sockbptr += cc;
         }

      }

      if (FD_ISSET(STDIN_FILENO, &inbits)) {
         ptycc = read(STDIN_FILENO, ptyibuf, sizeof(ptyibuf));
         if (ptycc < 0 && errno == EWOULDBLOCK) {
            ptycc = 0;
            errno = 0;
         }
      }

      if (ptycc > 0 && FD_ISSET(s, &outbits)) {
         ptybptr = ptyibuf;
         while (ptycc > 0) {
            cc = write(s, ptybptr, ptycc);
            ptycc -= cc;
            ptybptr += cc;
         }
      }

      if (FD_ISSET(ds, &inbits)) {
         return 0;
		}

      if (FD_ISSET(s, &errbits)) {
         while (!atoobmark) {
            ioctl(s, SIOCATMARK, &atoobmark);
            if (!atoobmark) {
               sockcc = read(s, sockibuf, 1);
               if (sockcc > 0)
                  write(STDOUT_FILENO, sockibuf, sockcc);
            }
         }

/*         recvdata(s, &oobparms, 1, MSG_OOB); */

         return 0;
      }

   }
}

/* ARGSUSED */
void plumber(int status)
{
   brokenpipe = 1;
}

void repair(void)
{
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &cliterm);
}

#if 0
int queuetransaction(int opmode, bool_t sendfiles, GENTRANSAREA *parameters)
{
   char opmodestr[OPMODELENGTH+1];
   char qfilename[FILENAMELENGTH];
   char qprefix[FILENAMELENGTH];
   char queuedir[FILENAMELENGTH];
   char queuetxfile[FILENAMELENGTH];
   char txfile[FILENAMELENGTH];
   char txdir[FILENAMELENGTH];
   char temp[FILENAMELENGTH];
   char newname[FILENAMELENGTH];
   char fullname[FILENAMELENGTH];
   DIR *dirp;
   struct dirent *entry;
   int qfd;
   int count;
   int found = 0;

   opmode -= 20;
   sprintf(opmodestr, "%02d", opmode);
   CtoCobol(parameters->opmode, opmodestr, sizeof(parameters->opmode));

   /* read INI files */
   GetPrivateProfileString("genserver", "queuedir", queuedir, sizeof(queuedir),
      "/IMOSDATA/2/QUEUES/GQDIR", WESCOMINI);
   GetPrivateProfileString("genserver", "queueprefix", qprefix,
      sizeof(qprefix), "genqueue", WESCOMINI);

   /* add trailing '/' if necessary */
   count = strlen(queuedir) - 1;
   if (*(queuedir + count) != '/' && count < FILENAMELENGTH - 1)
      strcat(queuedir, "/");

   strcpy(qfilename, queuedir);
   CobolToC(qfilename+strlen(qfilename), parameters->hostname, 
      sizeof(parameters->hostname)+1, sizeof(parameters->hostname));
   strcat(qfilename, "/");
   CheckDirectory(qfilename);

   memset(parameters->clientname, 32, sizeof(parameters->clientname));
   strncpy(parameters->clientname, "GQXMIT", 6);

   getqueuefilename(qfilename+strlen(qfilename), qprefix);

   if ((qfd = creat(qfilename, READWRITE)) < 0) {
      strncpy(parameters->status, "08", 2);
      return -1;
   }

   if (sendfiles) {
      CobolToC(temp, parameters->clientsendfile, sizeof(temp),
         sizeof(parameters->clientsendfile));
      ParseFilename(temp, txdir, txfile);
      if ((dirp = opendir(txdir)) == NULL) {
         CtoCobol(parameters->status, "08", sizeof(parameters->status));
         CtoCobol(parameters->error.name, "opening",
            sizeof(parameters->error.name));
         CtoCobol(parameters->error.description, txdir,
            sizeof(parameters->error.description));
         return -1;
      }

      while ((entry = readdir(dirp)) != NULL) {
         if (file_pattern_match(entry->d_name, txfile)) {
            sprintf(fullname, "%s/%s", txdir, entry->d_name);
            sprintf(queuetxfile, "%s.%s", qfilename, entry->d_name);
            if (!CopyFile(fullname, queuetxfile)) {
               CtoCobol(parameters->status, "08", sizeof(parameters->status));
               CtoCobol(parameters->error.name, "copying",
                  sizeof(parameters->error.name));
               CtoCobol(parameters->error.description, entry->d_name,
                  sizeof(parameters->error.description));
               return -1;
            }
            found = 1;
         }
      }

      if (!found) {
         CtoCobol(parameters->status, "08", sizeof(parameters->status));
         CtoCobol(parameters->error.name, "not found",
            sizeof(parameters->error.name));
         CtoCobol(parameters->error.description, temp,
            sizeof(parameters->error.description));
         return -1;
      }

      sprintf(queuetxfile, "%s.%s", qfilename, txfile);
      memset(parameters->clientsendfile, 32,
         sizeof(parameters->clientsendfile));
      CtoCobol(parameters->clientsendfile, queuetxfile,
         sizeof(parameters->clientsendfile));
   }

   count = sizeof(GENTRANSAREA);
   if (writen(qfd, parameters, count) < count) {
      CtoCobol(parameters->status, "08", sizeof(parameters->status));
      CtoCobol(parameters->error.name, "genqueue",
         sizeof(parameters->error.name));
      CtoCobol(parameters->error.description, "write error",
         sizeof(parameters->error.description));
   }

   close(qfd);

   strcpy(newname, qfilename);
   strcat(newname, ".gendata");

   rename(qfilename, newname);

   return 0;
}
#endif

int sendevar(int s, char *evar)
{
   char *data;
   char buffer[_POSIX_ARG_MAX];

   if (*evar == 0)
      return sendpacket(s, (void *)evar, (short)1);
   else {
      if ((data = getenv(evar)) == NULL)
         return -1;
      else {
         sprintf(buffer, "%s=%s", evar, data);
         return sendpacket(s, (void *)buffer, (short)strlen(buffer)+1);
      }
   }
}
