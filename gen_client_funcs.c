#if defined(_WIN32)
#   include <windows.h>
#endif      /* defined(_WIN32) */

#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if !defined(_WIN32)
#   include <sys/time.h>
#   include <sys/socket.h>
#   include <sys/stat.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#   include <sys/ttold.h>
#endif


#   if !defined(__hpux) && !defined(_GNU_SOURCE)
#      include <sys/sockio.h>
#   endif    /* !defined(__hpux) */

#   include <netinet/in.h>
#   include <unistd.h>
#   include <signal.h>
#   include <netdb.h>
#   include <fcntl.h>

#ifdef _GNU_SOURCE
#include <pty.h>
struct ncr_termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t     c_cc[19];
};
#else
#   include <termios.h>
#endif

#   include <libgen.h>
#   include <dirent.h>
#   include <limits.h>
#   include "msdefs.h"
#   include "readini.h"
#endif       /* !defined(_WIN32) */

#include "common.h"
#include "generic.h"
#include "gen2.h"
#include "gen_client_funcs.h"
#include "gen_common_funcs.h"
#include "wesnet.h"
#include "ws_syslog.h"

int client_put_file(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
   char szFileName[FILENAMELENGTH + 1];
   char szCopyFileName[FILENAMELENGTH + 1];
   PGPACKET pgPacket;
   char * lpOwner;
   char * lpGroup;
   char * lpFileBase;
   char szQPrefix[FILENAMELENGTH];
   char szTrueName[FILENAMELENGTH];
   int32_t dwSeconds;
   int32_t dwMilliSeconds;
   int nResult;

#if defined(_WIN32)
   GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_QPREFIX_KEY,
      GENSERVER_DEFAULT_QPREFIX, szQPrefix, sizeof(szQPrefix),
      WESCOM_INI_FILE);
#else
   GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_QPREFIX_KEY,
      szQPrefix, sizeof(szQPrefix), GENSERVER_DEFAULT_QPREFIX, 
      WESCOM_INI_FILE);
#endif

   memset(&pgPacket, 0, sizeof(pgPacket));
   RemoteFiletoPG(&pgPacket, &(lpGtaParms->hostrecvfile));

   if (pgPacket.filename[0] <= 32) {
      CobolToC(szFileName, lpGtaParms->clientsendfile, sizeof(szFileName),
         sizeof(lpGtaParms->clientsendfile));
      wesco_lstrcpy(szCopyFileName, szFileName);
      lpFileBase = ws_basename(szCopyFileName);

      /* check to see if the filename starts with the queue prefix */
      if (!strncmp(lpFileBase, szQPrefix, strlen(szQPrefix))) {
         sscanf(lpFileBase+strlen(szQPrefix)+1, "%lu.%lu.%s", &dwSeconds,
            &dwMilliSeconds, szTrueName);
      }
      else
         wesco_lstrcpy(szTrueName, lpFileBase);

      wesco_lstrcpy(pgPacket.filename, szTrueName);
   }
   else {
      wesco_lstrcpy(szFileName, pgPacket.filename);

      memset(pgPacket.filename, 0, sizeof(pgPacket.filename));
      strcpy(pgPacket.filename, ws_basename(szFileName));
   }

   lpOwner = (pgPacket.owner[0] == 0) ? NULL : pgPacket.owner;
   lpGroup = (pgPacket.group[0] == 0) ? NULL : pgPacket.group;

   nResult = gs_send_pg_packet(s, _GEN_PUT, pgPacket.filename,
      pgPacket.permissions, lpOwner, lpGroup);
   if (nResult < 0) {
      generr = ESEND;
      return -1;
   }

   CobolToC(szFileName, lpGtaParms->clientsendfile, sizeof(szFileName), 
      sizeof(lpGtaParms->clientsendfile));
   if (gs_put_file(s, szFileName, 0, NULL, NULL) < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int client_get_file(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
   PGPACKET pgPacket;
   char * lpOwner;
   char * lpGroup;
   char szFileName[FILENAMELENGTH + 1];
   int nResult;

   memset(&pgPacket, 0, sizeof(pgPacket));
   RemoteFiletoPG(&pgPacket, &(lpGtaParms->clientrecvfile));

   CobolToC(szFileName, lpGtaParms->hostsendfile, sizeof(szFileName), 
      sizeof(lpGtaParms->hostsendfile));
   nResult = gs_send_pg_packet(s, _GEN_GET, szFileName, pgPacket.permissions,
      pgPacket.owner, pgPacket.group);
   if (nResult < 0) {
      generr = ESEND;
      return -1;
   }

   lpOwner = (pgPacket.owner[0] <= 32) ? NULL : pgPacket.owner;
   lpGroup = (pgPacket.group[0] <= 32) ? NULL : pgPacket.group;

   nResult = gs_get_file(s, pgPacket.filename, pgPacket.permissions, lpOwner,
      lpGroup);
   if (nResult < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int client_send_run(wesco_socket_t s, LPGENTRANSAREA lpGtaParms, int heartbeat)
{
	char * cfg;

#if defined(_WIN32)
   bool_t bRedirect = FALSE;
#else
	int to = 0;
	int lto;     /* listen() time-out value */
   bool_t bRedirect;
	int flags;
   int16_t count;
   wesco_socket_t ios;
   unsigned short sport = IPPORT_RESERVED;
   wesco_socket_t cs;
   struct sockaddr_in pa;
   SOCKLEN_T size;

#ifdef _GNU_SOURCE
	struct ncr_termios netterm;
#else
	struct termios netterm;
#endif

	struct termios cliterm;
	struct winsize cliwin;
	char *evar_list[] = {
		"HICOLOR",
		"TERM",
		"NOVATERM",
		""
	};

   switch (lpGtaParms->redirect) {
      case '1':
         bRedirect = TRUE;
         break;
      default:
         bRedirect = FALSE;
         break;
    }
#endif      /* !defined(_WIN32) */

	cfg = get_cfg_file(NULL, (size_t)0);

   if (gs_send_run(s, (short)bRedirect, lpGtaParms) < 0) {
      generr = ESEND;
      return -1;
   }

#if !defined(_WIN32)
   if (bRedirect) {
      bRedirect ^= 0x8000;

      if ((cs = rsvport(&sport)) < 0)
         return -1;

		if (isatty(STDOUT_FILENO)) {
			tcgetattr(STDOUT_FILENO, &cliterm);
/*			memcpy(&netterm, &cliterm, sizeof(cliterm)); */
			netterm.c_iflag = htonl(cliterm.c_iflag);
			netterm.c_oflag = htonl(cliterm.c_oflag);
			netterm.c_cflag = htonl(cliterm.c_cflag);
			netterm.c_lflag = htonl(cliterm.c_lflag);
			memcpy(netterm.c_cc, cliterm.c_cc, 19);
		}
		else {
			memset(netterm.c_cc, 0, sizeof(netterm.c_cc));
			netterm.c_iflag = htonl(BRKINT|ISTRIP|ICRNL|IXON);
			netterm.c_oflag = htonl(OPOST|ONLCR|TABDLY);
			netterm.c_lflag = htonl(ISIG|ICANON|ECHO|ECHOE|ECHOK);
			netterm.c_cflag = htonl(B9600|CS8|CREAD|HUPCL);

			netterm.c_cc[VINTR] = 003;		/* Ctrl + C (^C) */
			netterm.c_cc[VQUIT] = 034;		/* Ctrl + \\ (^\\) */
			netterm.c_cc[VERASE] = 010;	/* Ctrl + H (^H) */
			netterm.c_cc[VKILL] = '@'; 	/* @ */
			netterm.c_cc[VEOF] = 004;		/* Ctrl + D (^D) */
			netterm.c_cc[VEOL] = 0;			/* Ctrl + @ (^@) */
			netterm.c_cc[VSTART] = 021;	/* Ctrl + Q (^Q) */
			netterm.c_cc[VSTOP] = 023;		/* Ctrl + S (^S) */
			netterm.c_cc[VSUSP] = 032;		/* Ctrl + Z (^Z) */
		}

#if defined(__hpux)
		netterm.c_reserved = htonl(cliterm.c_reserved);
#endif

      if (ws_send_packet(s, &netterm, sizeof(netterm)) <= 0)
         return -1;

		if (isatty(STDOUT_FILENO)) {
	      ioctl(STDOUT_FILENO, TIOCGWINSZ, &cliwin);
			cliwin.ws_row = htons(cliwin.ws_row);
			cliwin.ws_col = htons(cliwin.ws_col);
			cliwin.ws_xpixel = htons(cliwin.ws_xpixel);
			cliwin.ws_ypixel = htons(cliwin.ws_ypixel);
		}
		else {
			cliwin.ws_row = htons(24);
			cliwin.ws_col = htons(80);
			cliwin.ws_xpixel = htons(0);
			cliwin.ws_ypixel = htons(0);
		}

      if (ws_send_packet(s, &cliwin, sizeof(cliwin)) <= 0)
         return -1;

      count = -1;
      do {
         count++;
         gs_send_evar(s, evar_list[count]);
      } while (strcmp(evar_list[count], "") != 0);

      if (ws_send_packet(s, &sport, sizeof(sport)) <= 0)
         return -1;

		lto = GetPrivateProfileInt(GENCLIENT_INI_SECTION,
			GENCLIENT_LISTEN_TO_KEY, GENCLIENT_DEFAULT_LISTEN_TO, cfg);

      listen(cs, 1);
      size = sizeof(pa);

      if ((ios = ws_accept(cs, (struct sockaddr *)&pa, (SOCKLEN_T*)&size, lto)) < 0) {
         close(cs);
         return -1;
      }

      close(cs);

      setsocketkeepalive(ios, 1);
      setsocketkeepalive(s, 1);
      rawterminal();

		if (heartbeat != 0) {
			/* put code to get timeout value here */
			to = GetPrivateProfileInt(GENSERVER_INI_SECTION, "heartbeat",
				2, cfg);
		}

		/* set the non-blocking I/O flag */
		flags = fcntl(ios, F_GETFD);
		fcntl(ios, F_SETFD, flags|O_NONBLOCK);
		pty_client_io(ios, s, to, heartbeat);
		fcntl(ios, F_SETFD, flags);

      tcsetattr(STDOUT_FILENO, TCSANOW, &cliterm);
      setsocketkeepalive(s, 0);
      close(ios);
   }
#endif      /* !defined(_WIN32) */

   if (gs_get_gentransarea(s, lpGtaParms) < 0) {
      generr = ERECEIVE;
      return -1;
   }

   return 0;
}

int client_put_multiple_files(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
   char szFileName[FILENAMELENGTH + 1];
   PGPACKET pgPacket;
   char * lpOwner;
   char * lpGroup;
   int nResult;

   memset(&pgPacket, 0, sizeof(pgPacket));
   RemoteFiletoPG(&pgPacket, &(lpGtaParms->hostrecvfile));

   if (pgPacket.filename[0] <= 32) {
      CobolToC(pgPacket.filename, lpGtaParms->clientsendfile,
         sizeof(pgPacket.filename), sizeof(lpGtaParms->clientsendfile));
   }

   lpOwner = (pgPacket.owner[0] == 0) ? NULL : pgPacket.owner;
   lpGroup = (pgPacket.group[0] == 0) ? NULL : pgPacket.group;

   nResult = gs_send_pg_packet(s, _GEN_MPUT, pgPacket.filename,
   pgPacket.permissions, lpOwner, lpGroup);
   if (nResult < 0) {
      generr = ESEND;
      return -1;
   }

   CobolToC(szFileName, lpGtaParms->clientsendfile, sizeof(szFileName), 
      sizeof(lpGtaParms->clientsendfile));
   if (gs_put_multiple_files(s, szFileName) < 0) {
      generr = ESEND;
      return -1;
   }

   return 0;
}

int client_get_multiple_files(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
   PGPACKET pgPacket;
   char szFileName[FILENAMELENGTH + 1];
   int nResult;

   memset(&pgPacket, 0, sizeof(pgPacket));
   RemoteFiletoPG(&pgPacket, &(lpGtaParms->clientrecvfile));

   CobolToC(szFileName, lpGtaParms->hostsendfile, sizeof(szFileName), 
      sizeof(lpGtaParms->hostsendfile));
   nResult = gs_send_pg_packet(s, _GEN_MGET, szFileName, pgPacket.permissions,
      pgPacket.owner, pgPacket.group);
   if (nResult < 0) {
      generr = ESEND;
      return -1;
   }

   return ((gs_get_multiple_files(s) < 0) ? -1 : 0);
}

int client_get_ack(wesco_socket_t s, LPGENTRANSAREA lpGtaParms)
{
   char szBuffer[270];
   short *pOpCode, nCommand;
	LPACKPACKET lpAckPacket;
   LPERRORPACKET lpErrorPacket;

   if (ws_recv_packet(s, szBuffer, sizeof(szBuffer)) <= 0) {
      wesco_lstrcpyn(lpGtaParms->status, "03", 2);
      return -1;
   }

   pOpCode = (short *)szBuffer;
   if (ntohs(*pOpCode) == _GEN_ERROR) {
      lpErrorPacket = (ERRORPACKET *)szBuffer;
      strncpy(lpGtaParms->status, lpErrorPacket->status, 2);
      strncpy(lpGtaParms->error.description, lpErrorPacket->msg,
      strlen(lpErrorPacket->msg));
		ws_syslog(LOG_DEBUG, "ERROR <%s, %s>", lpErrorPacket->status,
			lpErrorPacket->msg);

      return -1;
   }
	else {
		lpAckPacket = (LPACKPACKET)szBuffer;
		nCommand = ntohs(lpAckPacket->acking);

		ws_syslog(LOG_DEBUG, "ACK <%d>", nCommand);
	}

   return 0;
}
