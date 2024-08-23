#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1997 WESCO, Inc.\nAll rights reserved.\n";
#endif

#if ! defined(__hpux)
#   define endianswap(x)  x
#   define lendianswap(x) x
#endif    /* __hpux */

#include <sys/types.h>
#include <sys/socket.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/ttold.h>
#endif

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <arpa/inet.h>

#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE) && !defined(_GNU_SOURCE)
#include <sys/select.h>

#ifdef _GNU_SOURCE
#include <asm/ioctls.h>
#else
#include <sys/filio.h>
#endif

#endif    /* _HPUX_SOURCE */

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#include <termios.h>
#endif

#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <utmp.h>

#if defined(_EMBEDDED_COBOL)
#include "sub.h"
#endif

#include "msdefs.h"
#include "common.h"
#include "generic.h"
#include "genservr.h"
#include "gen2.h"
#include "gen_common_funcs.h"
#include "gen_server_funcs.h"
#include "part.h"
#include "more.h"
#include "pty.h"
#include "err.h"
#include "termcmds.h"
#include "ac2.h"
#include "setsignal.h"

#ifndef MAX_RECEIVES
#define MAX_RECEIVES 50
#endif
#ifndef ARG_MAX
#define ARG_MAX			_POSIX_ARG_MAX
#endif
#define GS_BASE_OPTIONS "?adfi:s"

#if defined(_LIMIT_CONNECTIONS)
#define GS_LC_OPTIONS "c:m:rv"
#else
#define GS_LC_OPTIONS ""
#endif    /* defined(_LIMIT_CONNECTIONS) */

#define GS_OPTIONS GS_BASE_OPTIONS GS_LC_OPTIONS

char inbuffer[1024];
char outbuffer[1024];
int master;
char slave[20];
int deadchild = 0;
extern int errno;
struct termios cliterm;
struct winsize cliwin;
int corncob[2];
char debugfilename[FILENAMELENGTH];
int debugSocket;
bool_t brokenpipe = FALSE;
bool_t huprxd = FALSE;
char *inifile = WESCOM_INI_FILE;
int strict;
char szPartDir[FILENAMELENGTH+1];
char authdb[PATH_MAX];
int maxlisten = MAX_RECEIVES;

#if defined(_LIMIT_CONNECTIONS)
int semid;
int shmid;
int seminit[2] = { 0, 0 };
gendata_t * prgdata = (gendata_t *)NULL;
struct sembuf locksem[] = {
   { 0, 0, SEM_UNDO }, /* wait for semaphore #0 == 0  */
   { 0, 1, SEM_UNDO }  /* increment semaphore #0 by 1 */
};
#endif    /* defined(_LIMIT_CONNECTIONS) */

#if defined(_EMBEDDED_COBOL)
   Argument cobolargs[1];
#endif    /* _EMBEDDED_COBOL */

void sfuneral(int);
void sigchild(int);
void termhup(int);
int recvevar(int);
void cleanupdir(void);
void childs_funeral(int);
int get_gs_semaphore(void);
int set_maxconnections(int newvalue);
int set_numconnections(int newvalue);

char scookey[] = "\xff\xf0\xfc\xfd";

/*
 ****************************************************************************
 *
 *  Function:    main
 *
 *  Description: This function initializes the server.
 * 
 *  Parameters:  argc     - The number of command line arguments
 *               argv     - Pointers to the command line arguments
 *
 *  Returns:     Exit code
 *
 ****************************************************************************
*/
main(int argc, char *argv[])
{
   int option;
   extern char *optarg;
   short one = 1;
   int32_t maxcon = 0;
   int32_t count = 0;
   int32_t flags = 0;
    
   progName = argv[0];

   while ((option = getopt(argc, argv, GS_OPTIONS)) != EOF) {
      switch (option) {
         case '?':
            exit(0);
            break;
         case 'a':
            debugCobol = debugFile = debugSocket = TRUE;
            break;
         case 'd':
            debugCobol = TRUE;
            break;
         case 'f':
            debugFile = TRUE;
            break;
         case 'i':
            inifile = optarg;
            break;
         case 's':
            debugSocket = TRUE;
            break;
#if defined(_LIMIT_CONNECTIONS)
         case 'c':
            if (flags & GS_RESET_NUM_CONNECTIONS) {
               fprintf(stderr, "genserver: -c and -r are mutually exclusive\n");
               exit(1);
            }

            flags |= GS_SET_NUM_CONNECTIONS;
            count = atol(optarg);
            break;
         case 'm':
            flags |= GS_SET_MAX_CONNECTIONS;
            maxcon = atol(optarg);
            break;
         case 'r':
            if (flags & GS_SET_NUM_CONNECTIONS) {
               fprintf(stderr, "genserver: -c and -r are mutually exclusive\n");
               exit(1);
            }

            flags |= GS_RESET_NUM_CONNECTIONS;
            break;
         case 'v':
            flags |= GS_VIEW_SHARED_MEMORY;
            break;
#endif    /* defined(_LIMIT_CONNECTIONS) */
         default:
            fprintf(stderr, "usage: genserver -?adfs\n\n");
            exit(1);
      }
   }

   GetPrivateProfileString("genserver", "authdb", authdb, sizeof(authdb),
      GS_W2HOSTS, inifile);

   if (debugFile) {
      GetPrivateProfileString("genserver", "debugfile", debugfilename,
         sizeof(debugfilename), "debug.gen", inifile);

      if ((db = creat(debugfilename, DBGPERM)) == 0) {
         perror(progName);
         debugFile = FALSE;
      }
      else
         locallog(db, "INFO: Debug logging: ON\n");
   }
   else {
      db = open("/dev/null", O_RDONLY);
   }

#if defined(_LIMIT_CONNECTIONS)
   if ((semid = get_gs_semaphore()) < 0)
      exit(1);

   prgdata = (gendata_t *)GetSharedMemory(&shmid, SHM_GENSERVER,
      sizeof(gendata_t), IPC_CREAT|READWRITE);
   if (prgdata == NULL)
      err_sys("can not initialize shared memory");

   if (flags != 0) {
      if (flags & GS_SET_NUM_CONNECTIONS) {
         printf("Set numconnections = %d\n", count);
         set_numconnections(count);
      }

      if (flags & GS_SET_MAX_CONNECTIONS) {
         printf("Set maxconnections = %d\n", count);
         set_maxconnections(maxcon);
      }

      if (flags & GS_RESET_NUM_CONNECTIONS) {
         printf("Reset numconnections\n");
         set_numconnections(0);
      }

      if (flags & GS_VIEW_SHARED_MEMORY) {
         printf("Maximum Connections: %d\n", prgdata->maxconnections);
         printf("Current Connections: %d\n", prgdata->numconnections);
      }

      shmdt(prgdata);
      exit(0);
   }

   prgdata->maxconnections = GetPrivateProfileInt("genserver",
      "maxconnections", 150, inifile);
#endif    /* defined(_LIMIT_CONNECTIONS) */

   GetPrivateProfileString("genserver", "service", service, sizeof(service),
      "WDOCGEN", inifile);
   GetPrivateProfileString("genserver", "protocol", protocol,
      sizeof(protocol), "tcp", inifile);
   strict = GetPrivateProfileInt("genserver", "strict", 1, inifile);
   maxlisten = GetPrivateProfileInt("genserver", "listen-queue", MAX_RECEIVES,
      inifile);

   if (debugFile) {
      locallog(db, "INFO: service: %s\n", service);
      locallog(db, "INFO: protocol: %s\n", protocol);
      locallog(db, "INFO: strict: %d\n", strict);
   }

   if (initaddress(&myaddr_in, service, protocol) < 0) {
      wescolog(NETWORKLOG, 
         "FATAL:%s:%d:service lookup failed (%s/%s)",
         __FILE__, service, protocol);
      if (debugFile) {
         locallog(db,
            "%s: %s not found in /etc/services.  Server terminated!", 
            progName, service);
      }

      if (debugFile) {
         locallog(db, "FATAL:%s:%d:service lookup failed (%s/%s))\n",
            __FILE__, service, protocol);
      }

      err_sys("error on service lookup");
   }

   if (debugFile)
      locallog(db, "INFO: local address initialized\n");

   ls = getlistensocket(&myaddr_in, maxlisten);
   if (ls < 0)
      err_sys("error creating listen socket");
   
   if (debugFile)
      locallog(db, "INFO: listen socket established\n");
   
   setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
   
   daemoninit();
   
   if (debugFile)
      locallog(db, "INFO: daemon initialized\n");

   AcceptConnections();
   
   return 1;
}

/*
 ****************************************************************************
 *
 *  Function:    AcceptConnections
 *
 *  Description: This function sits in an infinite loop and listens to
 *               the specified port for a conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
void AcceptConnections(void)
{
   size_t addrLen;
   int count, attempt, serverCalled;
   char version[VERSIONLENGTH+1], *hostName;
   struct sigaction newact;
   struct sigaction oldact;
   struct sigaction genact;
   sigset_t zeromask;
   char *msg;
   int serrno;
   short one = 1;
   int socket_error;
   size_t so_size;
   int lock_fd;
   struct ac2 aci;
   int flags;
   sigset_t process_mask;
   sigset_t chld_mask;
   sigset_t hup_mask;

   if (debugFile)
      locallog(db, "INFO: setting up signal handlers\n");

   setsignal(SIGCHLD, childs_funeral, &oldact, SA_RESTART, 1, SIGALRM);
   setsignal(SIGHUP, SIG_IGN, NULL, 0, 0);

   sigemptyset(&zeromask);

   if (debugSocket) {
      setsocketdebug(ls, ON);

      if (debugFile)
         locallog(db, "INFO: socket-level debugging: ON\n");
   }

#if defined(_LIMIT_CONNECTIONS)
   prgdata->numconnections = 0;
#endif    /* defined(_LIMIT_CONNECTIONS) */

   sigemptyset(&process_mask);
   sigaddset(&process_mask, SIGHUP);
   sigaddset(&process_mask, SIGINT);
   sigaddset(&process_mask, SIGUSR1);
   sigaddset(&process_mask, SIGUSR2);
   sigaddset(&process_mask, SIGPIPE);

#if defined(SIGPOLL) && !defined(SIGIO)
   sigaddset(&process_mask, SIGPOLL);
#else
   sigaddset(&process_mask, SIGIO);
#endif

#if defined(SIGPROF)
   sigaddset(&process_mask, SIGPROF);
#endif

#if defined(SIGVTALRM)
   sigaddset(&process_mask, SIGVTALRM);
#endif

   sigemptyset(&hup_mask);
   sigaddset(&hup_mask, SIGHUP);

   sigemptyset(&chld_mask);
   sigaddset(&chld_mask, SIGCHLD);

   /*
      Set up an infinite loop and accept an incoming conversation.
      The function accept is a blocking call and waits until a
      conversation is received before returning.
   */
   for ( ; ; ) {
      sigprocmask(SIG_SETMASK, &process_mask, (sigset_t *)NULL);

      if (debugFile)
         locallog(db, "INFO: calling accept()\n");

      addrLen = sizeof(peeraddr_in);
      s = accept(ls, (struct sockaddr *)&peeraddr_in, (SOCKLEN_T *)&addrLen);
      sigprocmask(SIG_BLOCK, &chld_mask, &process_mask);
      if (s < 0) {
         serrno = errno;
         if (debugFile) {
            so_size = sizeof(socket_error);
            getsockopt(ls, SOL_SOCKET, SO_ERROR, (char *)&socket_error,
               (SOCKLEN_T *)&so_size);
            locallog(db, "ERROR: errno: %d: %s\n", serrno, strerror(serrno));
            locallog(db, "ERROR: socket errno: %d: %s\n", socket_error,
               strerror(socket_error));
         }

         if ((errno == EINTR) || (errno == EPROTO) || (errno == ECONNABORTED))
            continue;

         close(ls);
         wescolog(NETWORKLOG, "%s: unable to accept connection; errno = %d",
            progName, serrno);

         while ((ls = getlistensocket(&myaddr_in, maxlisten)) < 0) {
            serrno = errno;
            switch (ls) {
               case -1:
                  msg = "error creating socket";
                  break;
               case -2:
                  msg = "error binding socket";

                  /* we can try to reset the address for the server */
                  initaddress(&myaddr_in, service, protocol);
                  break;
               case -3:
                  msg = "error setting listen buffer";
                  break;
             }

             wescolog(NETWORKLOG, "%s: %s; errno = %d", progName,
                msg, serrno);

            if (debugFile) {
               so_size = sizeof(socket_error);
               getsockopt(ls, SOL_SOCKET, SO_ERROR, (char *)&socket_error,
                  (SOCKLEN_T *)&so_size);
               locallog(db, "ERROR: errno: %d: %s\n", errno, strerror(serrno));
               locallog(db, "ERROR: socket errno: %d: %s\n", socket_error,
                  strerror(socket_error));
            }

            sleep(30);    /* give it a rest */
         }

         if (debugSocket) {
            setsocketdebug(ls, ON);

            if (debugFile)
               locallog(db, "INFO: socket-level debugging: ON\n");
         }

         setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
      }
      else {
         lock_fd = open(WESCOM_LOCK_FILE, O_RDONLY);
         if (lock_fd >= 0) {
            /* do not allow a connection if branch is locked */
            close(lock_fd);
            close(s);
            continue;
         }

#if defined(_LIMIT_CONNECTIONS)
         if (increment_connection_counter() < 0) {
            close(s);
            continue;
         }
#endif    /* defined(_LIMIT_CONNECTIONS) */

         /*
            Fork the process.  If the process is a child process then call
            the Server function.  If the process is the parent close the
            client socket and resume the infinite loop.  If the return value
            of fork is a -1 the fork process failed.  Exit the program.
         */
         switch(fork()) {
            case -1:        /* forking error */
               close(s);
               serrno = errno;
               wescolog(NETWORKLOG, "%s: unable to fork daemon - connected\n",
                  progName);
               if (debugFile) {
                  locallog(db, "ERROR: Error occurred during fork()\n");
                  locallog(db, "       errno: %d: %s\n", serrno,
                     strerror(serrno));
                  locallog(db, "       Sleeping for 30 seconds.\n");
               }

               sleep(30);
               break;
            case 0:     /* child process */
               if (debugFile) {
                  locallog(db, "INFO: Connection accepted.\n");
                  locallog(db, "INFO: Authorizing connection.\n");
               }

               close(ls);

               sigprocmask(SIG_UNBLOCK, &hup_mask, &process_mask);

               setsignal(SIGCHLD, SIG_DFL, NULL, SA_RESTART, 0);
               setsignal(SIGHUP, SIG_DFL, NULL, 0, 0);

               attempt = 0;
               serverCalled = FALSE;
               setsid();

#if 0
               authorized = AuthorizeConnection(s, &hp, &peeraddr_in);
               if (authorized)
                  hostName = hp->h_name;
               else if (!strict) {
                  hostName = inet_ntoa(peeraddr_in.sin_addr);
                  authorized = gs_in_ip_family(authdb, hostName);
                  if (!authorized) {
                     requires_login = 1;
                     authorized = gs_in_ip_family(authdb, hostName);
                  }
               }
#endif

               memset(&aci, 0, sizeof(aci));
               hostName = inet_ntoa(peeraddr_in.sin_addr);
               AuthorizeConnection2(authdb, hostName, &aci);

               if (aci.authorized_to_connect == 1) {
                  if (aci.snapshot == 1) {
                     createsnapshot();
                  }

                  if (debugFile) {
                     locallog(db, "INFO: Authorization successful.\n");
                     locallog(db, "INFO: Host: %s.\n", aci.host_name);
                     locallog(db, "INFO: Negotiating release.\n");
                  }

                  while (!serverCalled && attempt < 2) {
                     memset(version, 0, sizeof(version));
                     if (WescoGetVersion(s, version, progName, aci.host_name)) {
                        if (debugFile)
                           locallog(db, "INFO: Received %s\n", version);
   
                        for (count = 0; (count < NUMVERSIONSUPPORTED)  &&
                             (!serverCalled); count++) {
                           if (!memcmp(srvrVersionTable[count].number, version, sizeof(version))) {
                              senddata( s, "OK      ", VERSIONLENGTH, 0);
                              if (debugFile)
                                 locallog(db, "INFO: Using version %s\n",
                                    version);
                              srvrVersionTable[count].Server(aci.host_name,
                                 srvrVersionTable[count].cobolProgram, 
                                 aci.requires_login,
                                 srvrVersionTable[count].heartbeat);
                              serverCalled = TRUE;

                              if (debugFile)
                                 locallog(db, "INFO: Terminating\n");
                           }
                           else {
                              if (debugFile)
                                 locallog(db, "INFO: %s is not supported\n", 
                                    version);
                           }
                        }
                     }
   
                     if (!serverCalled)
                        senddata(s, 
                           srvrVersionTable[NUMVERSIONSUPPORTED-1].number,
                           VERSIONLENGTH, 0);
   
                     attempt++;
                  }
               }
               else {
                  wescolog(NETWORKLOG,
                     "%s: host at address %s unknown.  Security disconnect!",
                     progName, aci.ip_address);
   
                  if (debugFile) {
                     locallog(db, "WARNING: Unknown host at address %s.\n",
                        aci.ip_address);
                     locallog(db, "         Security disconnect!\n");
                  }

                  close(s);
               }
   
               if (debugFile)
                  locallog(db, "INFO: Terminating\n");
   
               exit(0);
               break;
            default:    /* parent process */
               close(s);
         }
      }
   }
}

#define DELUSERDIR(x) { \
                         if (strcmp(x, "") != 0) { \
                            if (debugFile) \
                               locallog(db, "INFO: Deleting %s\n", x);\
                            DeletePartition(x); \
                            memset(x, 0, sizeof(x)); \
                         } \
                      }

/*
 ****************************************************************************
 *
 *  Function:    Server060400
 *
 *  Version:     06.04.00
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server060503(char *hostName, char *cobolFunc, int bufferSize, int heartbeat)
{
   char message[127];
   GENTRANSAREA parms;
   short *opcode;
   PGPACKET *pg;
   RUNPACKET *run;
   CDPACKET *cd;
   ERRORPACKET *ep;
   PARTPACKET *pp;
   char *ptr_owner;
   char *ptr_group;
   char tempdir[FILENAMELENGTH + 1];
   char partnumber[PARTITIONLENGTH + 1];
   pid_t childpid;
   int opt;
   int ret;
   sigset_t sigmask;
   sigset_t oldmask;
   struct sigaction newact;
   char switches[SWITCHLENGTH+1];
   short client_port;
   int client_s;
   int bytes;
   char ebuffer[26];
   mode_t filemode;
   int prog_timeout;
   char runprocess[FILENAMELENGTH+1];
#if defined(_HPUX_SOURCE)
   mode_t *hp_mode;
#endif

#if defined(NSC)
   char *ptr;
   char buffer[50];
#endif

#if defined(_EMBEDDED_COBOL)
   char *cobol_argv[3];
   int cobol_argc;
#elif defined(_CALL_COBOL)
   char buffer[50];
   char commandstr[HOSTPROCESSLENGTH+1];
#else
   char commandstr[HOSTPROCESSLENGTH+1];
#endif    /* _EMBEDDED_COBOL */

   /* set up SIGALRM handler */
   newact.sa_handler = TCPAlarm;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGPIPE);
#if defined(SIGCHLD)
   sigaddset(&newact.sa_mask, SIGCHLD);
#else
   sigaddset(&newact.sa_mask, SIGCLD);
#endif
   newact.sa_flags = 0;
   sigaction(SIGALRM, &newact, NULL);

   atexit(cleanupdir);

   if (debugFile)
      locallog(db, "INFO: Setting socket to linger for 2 seconds.\n");

   setsocketlinger(s, ON, 2);

   memset(partnumber, 0, sizeof(partnumber));
   memset(szPartDir, 0, sizeof(szPartDir));
   memset(inbuffer, 0, sizeof(inbuffer));
   memset(&parms, 0, sizeof(parms));

   if (debugFile)
      locallog(db, "INFO: Waiting for commands.\n");

   alarm(60);

   sigemptyset(&sigmask);
#if defined(SIGCHLD)
   sigaddset(&sigmask, SIGCHLD);
#else
   sigaddset(&sigmask, SIGCLD);
#endif
   sigprocmask(SIG_BLOCK, &sigmask, &oldmask);

   /* Loop while still receiving packets from client. */
   while ((ret = recvpacket(s, inbuffer, sizeof(inbuffer))) > 0) {
      if (debugFile)
         locallog(db, "INFO: Received command packet.\n");

      alarm(0);

      sigprocmask(SIG_SETMASK, &oldmask, NULL);

      if (debugFile)
         locallog(db, "INFO: Reset signal mask.\n");

      /* The opcode is the first two bytes in the packet. */
      opcode = (short *)inbuffer;

      if (debugFile)
         locallog(db, "INFO: Received opcode %02hu.\n", endianswap(*opcode));

      switch(endianswap(*opcode)) {
         case _GEN_PUT:
         /* 
            The put operation actually receives a file.  The "put"
            takes place from the client putting a file on the
            server.
         */
         pg = (PGPACKET *)inbuffer;

#if defined(_HPUX_SOURCE)
         hp_mode = (mode_t *)(&(pg->permissions));
         filemode = endianswap(*hp_mode);
#else
         filemode = pg->permissions;
#endif

         if (debugFile) {
            locallog(db, "INFO: PUT\n");
            locallog(db, "          FILENAME: %s\n", pg->filename);
            locallog(db, "          MODE:     %0004d\n", filemode);
            locallog(db, "          OWNER:    %s\n", pg->owner);
            locallog(db, "          GROUP:    %s\n", pg->group);
         }

         if (getfile(s, pg->filename, filemode, pg->owner, 
            pg->group) < 0) {
            ep = (ERRORPACKET *)outbuffer;

            memset(ep, 0, sizeof(ERRORPACKET));

            strncpy(ep->status, "14", 2);

            ep->opcode = ERROR;
            ep->gerrno = generr;
            strcpy(ep->msg, genmsg[generr % 1000]);

            sendpacket(s, ep, sizeof(ERRORPACKET));

            DELUSERDIR(szPartDir);

            return;
         }
         else
            sendack(s, _GEN_PUT);

         break;
      case _GEN_GET:
         /* 
            The get operation actually sends a file.  The "get"
            takes place from the client getting a file from the
            server.
         */
         pg = (PGPACKET *)inbuffer;

#if defined(_HPUX_SOURCE)
         hp_mode = (mode_t *)(&(pg->permissions));
         filemode = *hp_mode;
#else
         filemode = pg->permissions;
#endif

         if (debugFile) {
            locallog(db, "INFO: GET\n");
            locallog(db, "          FILENAME: %s\n", pg->filename);
            locallog(db, "          MODE:     %0004d\n", filemode);
            locallog(db, "          OWNER:    %s\n", pg->owner);
            locallog(db, "          GROUP:    %s\n", pg->group);
         }

         /* If the filename is empty don't do anything */
         if (pg->filename[0] == 0)
            break;

         if (pg->owner[0] == 0)
            ptr_owner = NULL;

         if (pg->group[0] == 0)
            ptr_group = NULL;

         if (putfile(s, pg->filename, filemode, ptr_owner, 
            ptr_group) == -1) {
            ep = (ERRORPACKET *)outbuffer;

            memset(ep, 0, sizeof(ERRORPACKET));

            strncpy(ep->status, "14", 2);
            ep->opcode = ERROR;
            ep->gerrno = generr;
            strcpy(ep->msg, genmsg[generr % 1000]);

            sendpacket(s, ep, sizeof(ERRORPACKET));

            DELUSERDIR(szPartDir);

            return;
         }
         else
            sendack(s, _GEN_GET);

         break;
      case _GEN_RUN:
         run = (RUNPACKET *)inbuffer;

         if (debugFile)
            locallog(db, "INFO: RUN\n");

         if (recvpacket(s, &parms, sizeof(GENTRANSAREA)) < 0) {
            close(s);

            DELUSERDIR(szPartDir);

            return;
         }

         if (debugFile)
            locallog(db, "INFO: Received GENTRANSAREA\n");

         if (partnumber[0] != 0)
            strncpy(parms.partition, partnumber, PARTITIONLENGTH);

#if defined(_EMBEDDED_COBOL)
         cobolargs[0].a_address = (char *)&parms;
         cobolargs[0].a_length = sizeof(parms);

         if (debugFile)
            locallog(db, "INFO: Initializing ACU-COBOL.\n");

         /* Initialize the cobol routines */
         cobol_argv[0] = progName;
         if (parms.switches[0] != 32) {
            /* 
               TRAFFICOP will come up in the debugger if the
               server is in debug mode and the io is to be
               redirected.
            */
            CobolToC(switches, parms.switches, sizeof(switches),
               sizeof(parms.switches));
            cobol_argv[1] = switches;
            cobol_argv[2] = cobolFunc;
            cobol_argc = 3;

            if (debugFile)
               locallog(db, "INFO: runcbl %s %s\n", switches, cobolFunc);
         }
         else {
            cobol_argv[1] = cobolFunc;
            cobol_argc = 2;

            if (debugFile)
               locallog(db, "INFO: runcbl %s\n", cobolFunc);
         }
#endif    /* _EMBEDDED_COBOL */

         if (run->redirect) {
            if (debugFile)
               locallog(db, "INFO: Redirecting I/O.\n");

            deadchild = 0;

            if (debugFile)
               locallog(db, "INFO: Receiving client terminal information.\n");

            if (recvpacket(s, &cliterm, sizeof(cliterm)) < 0) {
               close(s);

               DELUSERDIR(szPartDir);

               return;
            }

            cliterm.c_iflag = cliterm.c_iflag;
            cliterm.c_oflag = cliterm.c_oflag;
            cliterm.c_cflag = cliterm.c_cflag;
            cliterm.c_lflag = cliterm.c_lflag;

#if defined(_HPUX_SOURCE)
            cliterm.c_reserved = lendianswap(cliterm.c_reserved);
#endif

            if (debugFile)
               locallog(db, "INFO: Receiving client window information.\n");

            if (recvpacket(s, &cliwin, sizeof(cliwin)) < 0) {
               close(s);

               DELUSERDIR(szPartDir);

               return;
            }

            cliwin.ws_row = endianswap(cliwin.ws_row);
            cliwin.ws_col = endianswap(cliwin.ws_col);
            cliwin.ws_xpixel = endianswap(cliwin.ws_xpixel);
            cliwin.ws_ypixel = endianswap(cliwin.ws_ypixel);

            if (cliwin.ws_row == 0) cliwin.ws_row = 24;
            if (cliwin.ws_col == 0) cliwin.ws_col = 80;

            while (1) {
               if ((bytes = recvevar(s)) < 0) {
                  close(s);

                  DELUSERDIR(szPartDir);

                  return;
               }
               else if (bytes == 1)
                  break;
            }

            if (debugFile)
               locallog(db, "INFO: Setting up I/O socket.\n");

            if (recvpacket(s, &client_port, sizeof(client_port)) < 0) {
               close(s);
               DELUSERDIR(szPartDir);
               return;
            }

            spipe(corncob);

            switch ((childpid = pty_fork(&master, slave, &cliterm,
               &cliwin))) {
               case -1:
                  if (debugFile)
                     locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

                  strncpy(parms.status, "05", 2);
                  sendpacket(s, &parms, sizeof(GENTRANSAREA));
                  DELUSERDIR(szPartDir);
                  return;
               case 0:
                  if (debugFile)
                     locallog(db, "INFO: Terminal Process.\n");

#if defined(_EMBEDDED_COBOL)
#if defined(NSC)
   if ((ptr = calloc(20, sizeof(char))) == NULL) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parms.status, "05", 2);
      CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
      CtoCobol(parms.error.description, buffer,
         sizeof(parms.error.description));
      SendBuffer(progName, s, (char*)&parms, sizeof(parms));
      DELUSERDIR(szPartDir);
      return;
   }

   sprintf(ptr, "CLIENTNAME=x");
   strncpy(ptr+strlen(ptr), parms.clientname, 4);
   if (putenv(ptr) != 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parms.status, "05", 2);
      CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
      CtoCobol(parms.error.description, buffer,
         sizeof(parms.error.description));
      SendBuffer(progName, s, (char*)&parms, sizeof(parms));
      DELUSERDIR(szPartDir);
      return;
   }

   if (debugFile)
      locallog(db, "INFO: %s\n", ptr);
#endif
                  acu_initv(cobol_argc, cobol_argv);

                  if (debugFile)
                     locallog(db, "INFO: ACU-COBOL initialized!\n");

                  close(corncob[0]);
                  if ((ret = runcobol(&parms)) < 0 && debugFile)
                     locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

                  sleep(5);
                  close(s);
                  close(corncob[1]);

                  if (debugFile)
                     locallog(db, "INFO: Shutting down ACU-COBOL.\n");

                  acu_cancel(cobolFunc);
                  acu_shutdown(1);
                  _exit(0);
#elif defined(_CALL_COBOL)
   memset(switches, 0, sizeof(switches));
   CobolToC(switches, parms.switches, sizeof(switches),
      sizeof(parms.switches));
   sprintf(commandstr, "/usr/lbin/runcbl %s TRAFFICOP", switches);
   if (call_cobol(commandstr, "t_linkage", &parms, sizeof(parms)) < 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parms.status, "05", 2);
      CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
      CtoCobol(parms.error.description, buffer,
         sizeof(parms.error.description));
   }

   write(corncob[1], &parms, sizeof(GENTRANSAREA));
   sleep(5);
   close(s);
   close(corncob[1]);

   _exit(0);
#else
#if defined(NSC)
   if ((ptr = calloc(20, sizeof(char))) == NULL) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parms.status, "05", 2);
      CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
      CtoCobol(parms.error.description, buffer,
         sizeof(parms.error.description));
      SendBuffer(progName, s, (char*)&parms, sizeof(parms));
      DELUSERDIR(szPartDir);
      return;
   }

   sprintf(ptr, "CLIENTNAME=x");
   strncpy(ptr+strlen(ptr), parms.clientname, 4);
   if (putenv(ptr) != 0) {
      sprintf(buffer, "errno=%d", errno);
      strncpy(parms.status, "05", 2);
      CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
      CtoCobol(parms.error.description, buffer,
         sizeof(parms.error.description));
      SendBuffer(progName, s, (char*)&parms, sizeof(parms));
      DELUSERDIR(szPartDir);
      return;
   }

   if (debugFile)
      locallog(db, "INFO: %s\n", ptr);
#endif
                  /* set up for a system() call */
                  memset(commandstr, 0, sizeof(commandstr));
                  CobolToC(commandstr, parms.hostprocess, sizeof(commandstr),
                     sizeof(parms.hostprocess));
                  if (debugFile)
                     locallog(db, "%d: %s\n", getpid(), commandstr);
                  close(db);

                  system(commandstr);
/*                  execl(commandstr, "genshell", 0); */
#endif    /* _EMBEDDED_COBOL */
                  break;
               default:
                  if (debugFile)
                     locallog(db, "INFO: Parent process.\n");

                  if ((client_s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                     goto bad_call;

                  peeraddr_in.sin_port = client_port;

                  if (connect(client_s, (struct sockaddr *)&peeraddr_in,
                     sizeof(struct sockaddr)) < 0)
                     goto bad_call;

                  close(corncob[1]);
                  opt = 1;
                  ioctl(s, FIONBIO, &opt);
                  ioctl(client_s, FIONBIO, &opt);
                  noblock(master);

                  /* set up SIGCLD/SIGCHLD handler */
                  newact.sa_handler = sfuneral;
                  sigemptyset(&newact.sa_mask);
                  sigaddset(&newact.sa_mask, SIGALRM);
                  sigaddset(&newact.sa_mask, SIGPIPE);
                  sigaddset(&newact.sa_mask, SIGHUP);
                  newact.sa_flags = 0;
#if defined(SIGCHLD)
                  sigaction(SIGCHLD, &newact, NULL);
#else
                  sigaction(SIGCLD, &newact, NULL);
#endif

                  /* set up SIGHUP handler */
                  newact.sa_handler = termhup;
                  sigemptyset(&newact.sa_mask);
                  sigaddset(&newact.sa_mask, SIGALRM);
                  sigaddset(&newact.sa_mask, SIGPIPE);
                  sigaddset(&newact.sa_mask, SIGCHLD);
                  newact.sa_flags = 0;
                  sigaction(SIGHUP, &newact, NULL);

                  memset(runprocess, 0, sizeof(runprocess));
                  CobolToC(runprocess, parms.hostprocess, sizeof(runprocess),
                     sizeof(parms.hostprocess));
                  prog_timeout = GetPrivateProfileInt("genserver",
                     runprocess, 0, inifile);
                  nettopty(client_s, &parms, prog_timeout);
                  close(master);
                  opt = 0;
                  ioctl(s, FIONBIO, &opt);
                  close(corncob[0]);
               }
            }
            else {
               if (debugFile)
                  locallog(db, "INFO: No I/O redirection.\n");

               if (debugFile)
                  locallog(db, "INFO: Setting up login information.\n");

#if defined(_EMBEDDED_COBOL)
               acu_initv(cobol_argc, cobol_argv);

               if (debugFile)
                  locallog(db, "ACU-COBOL initialized!\n");

               if (cobol(cobolFunc, 1, cobolargs)) {
                  CtoCobol(parms.status, "05", 2);
                  sprintf(message, "%d", return_code);
                  CtoCobol(parms.error.name, message, sizeof(parms.error.name));
                  wescolog(NETWORKLOG, "%s: %s %s, return_code = %d", progName,
                     "error running COBOL program", cobolFunc, return_code);

                  if (debugFile)
                     locallog(db, "ERROR: ACU-COBOL error: %d\n", return_code);
               }

               if (debugFile)
                  locallog(db, "INFO: Shutting down ACU-COBOL.\n");

               acu_cancel(cobolFunc);
               acu_shutdown(1);
#elif defined(_CALL_COBOL)
   memset(switches, 0, sizeof(switches));
   CobolToC(switches, parms.switches, sizeof(switches),
      sizeof(parms.switches));
   sprintf(commandstr, "/usr/lbin/runcbl %s TRAFFICOP", switches);
               if (call_cobol(commandstr, "t_linkage", &parms, sizeof(parms)) < 0) {
                  sprintf(buffer, "errno=%d", errno);
                  strncpy(parms.status, "05", 2);
                  CtoCobol(parms.error.name, "system()", sizeof(parms.error.name));
                  CtoCobol(parms.error.description, buffer,
                     sizeof(parms.error.description));
                  SendBuffer(progName, s, (char*)&parms, sizeof(parms));
                  DELUSERDIR(szPartDir);
                  return;
            }
#else
               /* use the system call */
               memset(commandstr, 0, sizeof(commandstr));
               memcpy(commandstr, parms.hostprocess,
                  sizeof(parms.hostprocess));
               system(commandstr);
#endif    /* _EMBEDDED_COBOL */
            }

bad_call:
            ret = sendpacket(s, &parms, sizeof(parms));
            if (debugFile) {
               locallog(db, "INFO: Return Codes.\n");
               locallog(db, "      GTA-RET-CODE: %c%c\n",
                  parms.status[0], parms.status[1]);
               memset(ebuffer, 0, sizeof(ebuffer));
               memcpy(ebuffer, parms.error.name, sizeof(parms.error.name));
               locallog(db, "      GTA-RET-NAME: %s\n", ebuffer);
               memset(ebuffer, 0, sizeof(ebuffer));
               memcpy(ebuffer, parms.error.description,
                  sizeof(parms.error.description));
               locallog(db, "      GTA-RET-DESC: %s\n", ebuffer);
            }

            sendack(s, _GEN_RUN);

            break;
         case _GEN_CD:
            cd = (CDPACKET *)inbuffer;

            if (debugFile) {
               locallog(db, "INFO: CD %s\n");
               locallog(db, "        PATH: %s\n", cd->dirname);
            }

            if (CheckDirectory(cd->dirname) == DIRECTORY_ERROR) {
               ep = (ERRORPACKET *)outbuffer;

               memset(ep, 0, sizeof(ERRORPACKET));

               ep->opcode = ERROR;
               strncpy(ep->status, "15", 2);
               ep->gerrno = generr;
               strcpy(ep->msg, genmsg[generr % 1000]);

               sendpacket(s, ep, sizeof(ERRORPACKET));
               DELUSERDIR(szPartDir);
               return;
            }

            if (wescocd(cd->dirname) < 0) {
               ep = (ERRORPACKET *)outbuffer;                

               memset(ep, 0, sizeof(ERRORPACKET));

               ep->opcode = ERROR;
               strncpy(ep->status, "15", 2);
               ep->gerrno = generr;
               strcpy(ep->msg, genmsg[generr % 1000]);

               sendpacket(s, ep, sizeof(ERRORPACKET));
               DELUSERDIR(szPartDir);
               return;
            }
            else            
               sendack(s, _GEN_CD);

            break;
         case _GEN_MPUT:
            /* 
               The put operation actually receives a file.  The "put"
               takes place from the client putting a file on the
               server.
            */
            pg = (PGPACKET *)inbuffer;

            if (debugFile) {
               locallog(db, "INFO: MPUT %s\n");
               locallog(db, "          PATTERN: %s\n", pg->filename);
            }

            if (mget(s) < 0) {          
               ep = (ERRORPACKET *)outbuffer;

               memset(ep, 0, sizeof(ERRORPACKET));
               strncpy(ep->status, "14", 2);
               ep->opcode = ERROR;
               ep->gerrno = generr;
               strcpy(ep->msg, genmsg[generr % 1000]);

               if (debugFile)
                  locallog(db, "INFO: Sending error packet.\n");

               sendpacket(s, ep, sizeof(ERRORPACKET));
               DELUSERDIR(szPartDir);
               return;
            }
            else {
               if (debugFile)
                  locallog(db, "INFO: Sending command ACK.\n");

               sendack(s, _GEN_MPUT);
            }

            break;
         case _GEN_MGET:
            /* 
               The get operation actually sends a file.  The "get"
               takes place from the client getting a file from the
               server.
            */
            pg = (PGPACKET *)inbuffer;

            if (debugFile) {
               locallog(db, "INFO: MGET\n");
               locallog(db, "          PATTERN: %s\n", pg->filename);
            }

            /* If the filename is empty don't do anything */
            if (pg->filename[0] == 0)
               break;

            if (pg->owner[0] == 0)
               ptr_owner = NULL;

            if (pg->group[0] == 0)
               ptr_group = NULL;

            if (mput(s, pg->filename) < 0) {
               close(s);
               DELUSERDIR(szPartDir);
               return;
            }
            else
               sendack(s, _GEN_MGET);

            break;
         case _GEN_CRPART:
            pp = (PARTPACKET *)inbuffer;

            if (debugFile)
               locallog(db, "INFO: CRPART\n");

            if (!GetPartition(partnumber, pp->func, szPartDir)) {
               ep = (ERRORPACKET *)outbuffer;

               memset(ep, 0, sizeof(ERRORPACKET));
               strncpy(ep->status, "11", 2);
               ep->opcode = ERROR;

               if (debugFile)
                  locallog(db, "INFO: Sending ERROR packet.\n");

               sendpacket(s, ep, sizeof(ERRORPACKET));
               DELUSERDIR(szPartDir);
               return;
            }

            sprintf(tempdir, "%s.%d", szPartDir, getpid());
            if (!rename(szPartDir, tempdir))
               strcpy(szPartDir, tempdir);

            if (debugFile)
               locallog(db, "INFO: Created directory: %s\n", szPartDir);

            if (chdir(szPartDir)) {
               if (debugFile)
                  locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

               ep = (ERRORPACKET *)outbuffer;

               memset(ep, 0, sizeof(ERRORPACKET));
               strncpy(ep->status, "15", 2);
               ep->opcode = ERROR;

               if (debugFile)
                  locallog(db, "INFO: Sending error packet.\n");

               sendpacket(s, ep, sizeof(ERRORPACKET));
               DELUSERDIR(szPartDir);
               return;
            }
            else {
               if (debugFile)
                  locallog(db, "INFO: Sending command ACK.\n");

               sendack(s, _GEN_CRPART);
            }

            break;
         case _GEN_DELPART:
      pp = (PARTPACKET *)inbuffer;

      if (debugFile)
          locallog(db, "INFO: Deleting directory %s.\n", szPartDir);

      DeletePartition(szPartDir);
      sendack(s, _GEN_DELPART);

      break;
            case _GEN_DONE:
               if (debugFile)
                  locallog(db, "INFO: DONE.\n");

               close(s);
               return;
       default:
         DELUSERDIR(szPartDir);
         if (debugFile)
            locallog(db, "INFO: Invalid command.\n");

      break;
   }

   memset(inbuffer, 0, sizeof(inbuffer));
   if (debugFile)
           locallog(db, "INFO: Waiting for more commands.\n");

   alarm(60);
        sigprocmask(SIG_BLOCK, &sigmask, &oldmask);
    }

    DELUSERDIR(szPartDir);

    if (debugFile)
       locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));
}

/*
 ****************************************************************************
 *
 *  Function:    Server060400
 *
 *  Version:     06.04.00
 *
 *  Description: This function processes the conversation.
 * 
 *  Parameters:  None
 *
 *  Returns:     Nothing
 *
 ****************************************************************************
*/
/* ARGSUSED */
void Server080500(char *hostName, char *cobolFunc, int requires_login, int heartbeat)
{
   short *opcode;
   char tempdir[FILENAMELENGTH + 1];
   char partnumber[PARTITIONLENGTH + 1];
   int opt;
   int ret;
   sigset_t sigmask;
   sigset_t oldmask;
   struct sigaction newact;
   char szUserDir[FILENAMELENGTH+1];
   int login_attempts = 1;
   int logged_in = 0;

#if defined(NSC)
   char *ptr;
   char buffer[50];
#endif

#if defined(_EMBEDDED_COBOL)
   char *cobol_argv[3];
   int cobol_argc;
#elif defined(_CALL_COBOL)
   char buffer[50];
   char commandstr[HOSTPROCESSLENGTH+1];
#else
   char commandstr[HOSTPROCESSLENGTH+1];
#endif    /* _EMBEDDED_COBOL */

   memset(szUserDir, 0, sizeof(szUserDir));

   /* set up SIGALRM handler */
   newact.sa_handler = TCPAlarm;
   sigemptyset(&newact.sa_mask);
   sigaddset(&newact.sa_mask, SIGPIPE);
   sigaddset(&newact.sa_mask, SIGCLD);
   newact.sa_flags = 0;
   sigaction(SIGALRM, &newact, NULL);

   atexit(cleanupdir);

   if (debugFile)
      locallog(db, "INFO: Setting socket to linger for 2 seconds.\n");

   setsocketlinger(s, ON, 2);

   if (debugFile)
      locallog(db, "INFO: Waiting for commands.\n");

   alarm(60);

   sigemptyset(&sigmask);
   sigaddset(&sigmask, SIGCLD);
   sigprocmask(SIG_BLOCK, &sigmask, &oldmask);

   /* Loop while still receiving packets from client. */
   while ((ret = ws_recv_packet(s, inbuffer, sizeof(inbuffer))) > 0) {
      if (debugFile)
         locallog(db, "INFO: Received command packet.\n");

      alarm(0);

      sigprocmask(SIG_SETMASK, &oldmask, NULL);

      if (debugFile)
         locallog(db, "INFO: Reset signal mask.\n");

      /* The opcode is the first two bytes in the packet. */
      opcode = (short *)inbuffer;
      *opcode = ntohs(*opcode);

      if (debugFile)
         locallog(db, "INFO: Received opcode %02hu.\n", *opcode);

      if (requires_login && !logged_in) {
         if (*opcode != _GEN_LOGIN) {
            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }
      }

      switch(*opcode) {
      case _GEN_PUT:
         if (genserver_get_file(s, (LPPGPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: PUT failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: PUT successful\n");

         break;
      case _GEN_GET:
         if (genserver_put_file(s, (LPPGPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: GET failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: GET successful\n");

         break;
      case _GEN_RUN:
         if (genserver_run(s, (LPRUNPACKET)inbuffer, szUserDir, heartbeat)) {
            if (debugFile)
               locallog(db, "ERROR: RUN failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: RUN successful\n");

         break;
      case _GEN_CD:
         if (genserver_chdir(s, (LPCDPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: CD failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: CD successful\n");

         break;
      case _GEN_MPUT:
         if (genserver_get_multiple_files(s, (LPPGPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: MPUT failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: MPUT successful\n");

         break;
      case _GEN_MGET:
         if (genserver_put_multiple_files(s, (LPPGPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: MGET failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: MGET successful\n");

         break;
      case _GEN_CRPART:
         if (genserver_create_user_directory(s, (LPPARTPACKET)inbuffer, szUserDir)) {
            if (debugFile)
               locallog(db, "ERROR: CRPART failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: CRPART successful\n");

         break;
      case _GEN_DELPART:
         if (genserver_delete_user_directory(s, szUserDir)) {
            if (debugFile)
               locallog(db, "ERROR: DELPART failed\n");

            DELUSERDIR(szPartDir);
            wesco_closesocket(s);
            return;
         }

         if (debugFile)
            locallog(db, "INFO: DELPART successful\n");

         break;
      case _GEN_DONE:
         gs_send_ack_packet(s, _GEN_DONE);
         DELUSERDIR(szPartDir);
         wesco_closesocket(s);

         if (debugFile)
            locallog(db, "INFO: DONE successful\n");

         return;
      case _GEN_LOGIN:
         if (gs_check_login(s, (LPPWDPACKET)inbuffer)) {
            if (debugFile)
               locallog(db, "ERROR: LOGIN failed\n");

            login_attempts++;
            if (login_attempts > 3) {
               wescolog(NETWORKLOG, "Too many login attempts");
               DELUSERDIR(szPartDir);
               wesco_closesocket(s);
               return;
            }

            break;
         }

         if (debugFile)
            locallog(db, "INFO: LOGIN successful\n");

         logged_in = 1;

         break;
      case _GEN_HRTBEAT:
         gs_send_ack_packet(s, *opcode);
         break;
      default:
         if (debugFile)
            locallog(db, "ERROR: Unknown Command: %02hu\n", *opcode);

         break;
      }

      if (debugFile)
         locallog(db, "INFO: memset(inbuffer, 0);\n");

      memset(inbuffer, 0, sizeof(inbuffer));
      if (debugFile)
         locallog(db, "INFO: Waiting for more commands.\n");

      alarm(60);
      sigprocmask(SIG_BLOCK, &sigmask, &oldmask);
   }

   DELUSERDIR(szPartDir);

   if (debugFile)
      locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));
}

void Server080501(char *hostName, char *cobolFunc, int requires_login, int heartbeat)
{
   int result;
   short rl = (short)requires_login; /* ensures conversion to short */

   result = ws_send_packet(s, (wesco_void_ptr_t)&rl, (size_t)sizeof(short));
   if (result < 0)
      return;
   
   Server080500(hostName, cobolFunc, requires_login, heartbeat);
}

/* ARGSUSED */
void sfuneral(int trash)
{
   if (debugFile)
      locallog(db, "INFO: Received SIGCLD.\n");

   deadchild = TRUE;
}

void termhup(int trash)
{
   if (debugFile)
      locallog(db, "INFO: Received SIGHUP.\n");

   huprxd = TRUE;
}

int nettopty(int s, GENTRANSAREA *parms, int timeout)
{
   int maxfd, cc;
   register int ptycc, sockcc;
   char ptyibuf[1024], sockibuf[1024], *ptybptr, *sockbptr, *ptr;
   fd_set inbits, outbits, errbits, *ob;
   struct timeval stimeout;
   struct timeval *ptimeout;
   int index, num_fds_ready;
   char *pTerm = getenv("TERM");
   struct termcmds term_command[4] = {
      { NCR_7902_CLEAR, NCR_7902_RMSO },
      { VT100_CLEAR, VT100_RMSO },
      { ADDS_CLEAR, ADDS_RMSO },
      { AT386_CLEAR, AT386_RMSO }
   };

   if (pTerm == NULL)
      pTerm = "vt100";

   for (ptr = pTerm; *pTerm != 0; pTerm++)
      *ptr = tolower(*ptr);

   if (!strcmp(pTerm, TC_7902_4902_TERM))
      index = TC_NCR_7902_INDEX;
   else if (!strcmp(pTerm, TC_ADDS_TERM))
      index = TC_ADDS_INDEX;
   else if (!strcmp(pTerm, TC_AT386_TERM))
      index = TC_AT386_INDEX;
   else /* default to a vt100 */
      index = TC_VT100_INDEX;

   if (debugFile) {
      locallog(db, "INFO: index into term_command = %d\n", index);
   }

   if (timeout <= 0)
      ptimeout = (struct timeval *)NULL;
   else {
      stimeout.tv_sec = timeout * 60;
      stimeout.tv_usec = 0;
      ptimeout = &stimeout;
   }

   ptycc = sockcc = 0;

   maxfd = 1 + FOPEN_MAX;

   FD_ZERO(&inbits);
   FD_ZERO(&outbits);
   FD_ZERO(&errbits);

   for ( ; ; ) {
      FD_ZERO(&inbits);
      FD_ZERO(&outbits);

      ob = (fd_set *)NULL;

      if (sockcc > 0) {
         FD_SET(master, &outbits);
         ob = &outbits;
      }
      else
         FD_SET(s, &inbits);

      if (ptycc > 0) {
         FD_SET(s, &outbits);
         ob = &outbits;
      }
      else
         FD_SET(master, &inbits);

      FD_SET(corncob[0], &inbits);
      FD_SET(master, &errbits);

      num_fds_ready = select(maxfd,&inbits,ob,&errbits, ptimeout);
      if (debugFile) {
         locallog(db, "INFO: num_fds_ready = %d\n", num_fds_ready);
      }

      if (num_fds_ready <= 0) {
         if ((num_fds_ready < 0) && (errno == EINTR)) {
            if (debugFile) {
               locallog(db, "INFO: errno == EINTR; continuing loop\n");
            }

            continue;
         }

         if (num_fds_ready == 0) {
            if (debugFile) {
               locallog(db, "INFO: write term_command[%d].tc_clear\n", index);
            }

            write(s, term_command[index].tc_clear,
               strlen(term_command[index].tc_clear));
            if (debugFile) {
               locallog(db, "INFO: write term_command[%d].tc_rmso\n", index);
            }

            write(s, term_command[index].tc_rmso,
               strlen(term_command[index].tc_rmso));
         }

         if (debugFile) {
            locallog(db, "INFO: sending OOB data\n");
         }

         senddata(s, "1", 1, MSG_OOB);
         return -1;
      }

      /*
       **********************************************************************
       *
       *  If the child process dies without sending the out-of-band data and
       *  the COBOL parameters, then the parent must send the out-of-band
       *  data to the client.  This informs the client that data on the
       *  socket should no longer be rerouted to the screen.
       *
       **********************************************************************
      */
      if (deadchild || brokenpipe || huprxd) {
         senddata(s, "1", 1, MSG_OOB);
         strncpy(parms->status, "05", 2);

         if (debugFile)
            locallog(db, "INFO: SIGCHLD received.  No parameters.\n");

         return 0;
      }

      if (FD_ISSET(corncob[0], &inbits)) {
         senddata(s, "1", 1, MSG_OOB);
         read(corncob[0], parms, sizeof(GENTRANSAREA));

         if (debugFile)
            locallog(db, "reading parameters from child\n");

         return 0;
      }

      if (FD_ISSET(s, &inbits)) {
         sockcc = read(s, sockibuf, sizeof(sockibuf));
         if (sockcc < 0 && errno == EWOULDBLOCK) {
            sockcc = 0;
            errno = 0;
         }
         else if (sockcc <= 0)
            return -1;
      }

      if (sockcc > 0 && FD_ISSET(master, &outbits)) {
         sockbptr = sockibuf;
         while (sockcc > 0) {
            cc = write(master, sockbptr, sockcc);
            sockcc -= cc;
            sockbptr += cc;
         }
      }

      if (FD_ISSET(master, &inbits)) {
         ptycc = read(master, ptyibuf, sizeof(ptyibuf));
         if (ptycc < 0 && errno == EWOULDBLOCK) {
            ptycc = 0;
            errno = 0;
         }
         else if (ptycc <= 0)
            return -1;
      }

      if (ptycc > 0 && FD_ISSET(s, &outbits)) {
         ptybptr = ptyibuf;
         while (ptycc > 0) {
            cc = write(s, ptybptr, ptycc);
            ptycc -= cc;
            ptybptr += cc;
         }
      }
   }
}

#if defined(_EMBEDDED_COBOL)
int runcobol(GENTRANSAREA *parms)
{
   char message[128];

   if (cobol(cobolFunc, 1, cobolargs)) {
      CtoCobol(parms->status, "05", 2);
      sprintf(message, "%d", return_code);
      CtoCobol(parms->error.name, message, ERRORNAMELENGTH);
      wescolog(NETWORKLOG,
         "%s: error running COBOL program %s, return_code = %d",
         progName, cobolFunc, return_code);
   }

   if (debugFile)
      locallog(db, "INFO: ACU-COBOL execution complete.\n");

   if (write(corncob[1], parms, sizeof(GENTRANSAREA)) < 0)
      return -2;

   return 0;
}
#endif    /* _EMBEDDED_COBOL */

void sigchild(int status)
{
   pid_t pid;
   int stati;
   char buf[5];

   while ((pid = waitpid(-1, &stati, WNOHANG)) > 0);
}

int recvevar(int s)
{
   char buffer[ARG_MAX];
   char *tmp;
   int bytes;

   if ((bytes = recvpacket(s, buffer, (short)sizeof(buffer))) < 0) {
      if (debugFile)
         locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

      return -1;
   }
   else if (buffer[0] == 0) {
      if (debugFile)
         locallog(db, "INFO: Environmental variables set.\n");

      return 1;    /* this means the end of the list */
   }
   else {
      if ((tmp = calloc(bytes + sizeof(char), sizeof(char))) == NULL) {
         if (debugFile)
            locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

         return -1;
      }

      memcpy(tmp, buffer, (size_t)bytes);

      if (debugFile)
         locallog(db, "INFO: %s\n", tmp);
      if (putenv(tmp) != 0) {
         if (debugFile)
            locallog(db, "ERROR: %d: %s\n", errno, strerror(errno));

         return -1;
      }

      return bytes;
   }
}

void cleanupdir(void)
{
   DELUSERDIR(szPartDir);
   return;
}

void childs_funeral(int signo)
{
   int status;
   int savecore;
   pid_t child_pid;
   char mailto[256];
   char cmdline[512];
   struct utsname un;
   struct sembuf locksem[] = {
      { 0, 0, SEM_UNDO }, /* wait for semaphore #0 == 0  */
      { 0, 1, SEM_UNDO }  /* increment semaphore #0 by 1 */
   };

   while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
      savecore = GetPrivateProfileInt("genserver", "save-core", 0, inifile);
      if (WCOREDUMP(status) && savecore) {
         GetPrivateProfileString("genserver", "dump-mail-to", mailto,
            sizeof(mailto), "root", inifile);
         wescolog(NETWORKLOG,
            "%d: child %d dumped core, preserving user dir if available.",
            getpid(), child_pid);

         uname(&un);
         sprintf(cmdline, "Subject: GenServer Core Dump\ngenserver process %d on %s dumped core",
            child_pid, un.sysname);
         mailmessage(mailto, cmdline);
         sprintf(cmdline, "cp -r /IMOSDATA/USER/[0-9][0-9][0-9]*.%d /mjb", child_pid);
         system(cmdline);
      }
      else {
         sprintf(cmdline, "rm -rf /IMOSDATA/USER/[0-9][0-9][0-9]*.%d", child_pid);
         system(cmdline);
      }

#if defined(_LIMIT_CONNECTIONS)
      semop(semid, locksem, 2);

      prgdata->numconnections--;
      if (debugFile) {
         wescolog(db, "%d: number of connections decreased to %d\n",
            getpid(), prgdata->numconnections);
      }

      semctl(semid, 0, SETVAL, 0);
#endif    /* defined(_LIMIT_CONNECTIONS) */
   }
}

#if defined(_LIMIT_CONNECTIONS)
int increment_connection_counter(void)
{
   int ret_val;
   struct sembuf locksem[] = {
      { 0, 0, SEM_UNDO }, /* wait for semaphore #0 == 0  */
      { 0, 1, SEM_UNDO }  /* increment semaphore #0 by 1 */
   };

   if (semop(semid, locksem, 2) < 0) {
      perror("genserver");
      ret_val = -1;
   }

   if (prgdata->numconnections < prgdata->maxconnections) {
      prgdata->numconnections++;
      ret_val = prgdata->numconnections;
   }
   else {
      if (debugFile) {
         locallog(db, "%d: too many current connections\n", getpid());
      }

      ret_val = -1;
   }

   semctl(semid, 0, SETVAL, 0);

   return ret_val;
}

int get_gs_semaphore(void)
{
   int flags = 0;
   int sid;

   flags = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
   if ((sid = semget(SEM_GENSERVER, 1, flags)) < 0) {
      flags |= IPC_CREAT;
      if ((sid = semget(SEM_GENSERVER, 1, flags)) < 0) {
         perror("genserver");
         return -1;
      }

      if (semctl(semid, 0, SETALL, seminit) < 0) {
         perror("genserver");
         return -1;
      }
   }

   return sid;
}

int set_maxconnections(int newvalue)
{
   int ret_val;

   if (semop(semid, locksem, 2) < 0) {
      wescolog(NETWORKLOG, "%s:%s:%s", __FILE__, __LINE__,
         strerror(errno));
      return -1;
   }

   prgdata->maxconnections = newvalue;

   semctl(semid, 0, SETVAL, 0);

   return ret_val;
}

int set_numconnections(int newvalue)
{
   int ret_val;

   if (semop(semid, locksem, 2) < 0) {
      wescolog(NETWORKLOG, "%s:%s:%s", __FILE__, __LINE__,
         strerror(errno));
      return -1;
   }

   prgdata->numconnections = newvalue;

   semctl(semid, 0, SETVAL, 0);

   return ret_val;
}
#endif    /* defined(_LIMIT_CONNECTIONS) */
