#define NORMAL /* 

WESCO - Confidential and Proprietary

Copyright (c) 1994 WESCO 
All rights reserved.                                               

Written under contract for WESCO by:

BekTek, Inc. 
5949 Pudding Stone Lane
Bethel Park, PA  15102
Phone: (412) 835-2994
Fax:   (412) 833-9482

Author: Harold E. Price

client.c - main routine for branch receive program.

usage:  wsdial -lpath -pnumber -fpath -brate -sscript
  -lpath   path is line name
  -xpath   path is directory for file xfer
  -brate   rate is baud rate
  -sscript script is alternate command/response script file
  -ddebug  debug is debug value
  -m       make directories only, then exit
  -c       exit immediately if CTS not asserted.
  -k       kill net_client

Modifications
HEP 12/9/94  - V0.3
               call get_dcd in script.c before each operation, to cause
               the line to cycle if DCD has bobbled - new USR modems seem
               to diddle DCD on ATZ.
MJB 9/15/98  - V2.0
               add code to kill the net_client program for NetPolling

*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifndef DOS
#include <fcntl.h>

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#include <termios.h>
#endif


#ifdef _GNU_SOURCE
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#else
#include <macros.h>
#endif

#include <signal.h>

#ifdef _GNU_SOURCE
#include <signal.h>
#else
#include <siginfo.h>
#endif

#include <limits.h>
#endif
#include "client.h"
#include "cdial.h"
#include "line.h"
#define WRITE_SIZE 500

#define printf tprintf
#define fprintf tfprintf


#define MASK(f) (1<<(f))

char outbuf[WRITE_SIZE];
int NumOutFiles;
static char tbuf[120];
char *config_file = "/usr/hlcp/netpoll.ini";

FILE *verbose;
struct DIAL_PARMS *g_p;

extern int errno;
extern void sigdeath();
extern void sighup();
void debug_out(char *, char *);
void arg_error_out(char *, char *);
void main (int, char *[]);
void do_one(struct DIAL_PARMS *);
void error_out(struct DIAL_PARMS *,char *);
int wait_for_input(struct DIAL_PARMS *);
void sigusr1(int, struct siginfo *);
void sigusr2(int);
void sigwinch(int, struct siginfo *);
void kill_net_client(void);

int program_return_code = 0;

void debug_out(char *msg, char *s)
{
   int i;
   int len;

   len=strlen(s)-1;   /* scan all but notional last <lf> */
   for (i=0;i<len;i++) {
      if (s[i]=='\r') s[i]='%';
      else if (s[i]=='\n') s[i]='~';
   }
 
   if (s[i]=='\r') s[i]='%';   /* get trailing \r */

   fprintf(stdout,"%s%s\n",msg,s);
}

void error_out(struct DIAL_PARMS *p,char *s)
{
   fprintf(stdout,"LINE: %s ERROR: %s\n",stripline(p->line),s);
}


void arg_error_out(s,argv)
char *s;
char *argv;
{
   printf("ERROR: %s %s\n",s,argv);
   printf("usage:\n");
   printf("  -lpath   path is line name\n");
   printf("  -xpath   path is directory for file xfer\n");
   printf("  -brate   rate is baud rate\n");
   printf("  -sscript script is command/response script file\n");
   printf("  -ddebug  debug is debug value\n");
   printf("  -c       exit immediately if CTS not asserted\n");
   printf("  -k       kill net_client\n");
   printf("  -kfile   configuration file\n");
   exit(EXIT_BAD_PARM);
}



void main(argc,argv)
int argc;
char *argv[];
{
   int err;
   int i;
   int status;
   int len;
   char tmp[120];
   int ctsexit=0;
   char prepath[_POSIX_PATH_MAX];

#ifndef DOS
   struct termios termios;
   speed_t speed;
   struct timeval first, last, lapsed;
   struct sigaction sigact;    /** 50128 */
#endif

   int arg;
   struct DIAL_PARMS *p;

   verbose=stdout;

   printf("Client Data XFER version 0.3 running\n");
#ifndef DOS
   signal(SIGHUP,sighup);
   signal(SIGINT,sigdeath);
   signal(SIGQUIT,sigdeath);
   signal(SIGILL,sigdeath);
   signal(SIGBUS,sigdeath);
   signal(SIGPIPE,sigdeath);
   signal(SIGTERM,sigdeath);

   /** 50128 */
   sigact.sa_handler = sigusr1;
   sigemptyset(&sigact.sa_mask);
#ifdef SA_SIGINFO
   sigact.sa_flags = SA_SIGINFO;
#else
   sigact.sa_flags = 0;
#endif
   sigaction(SIGUSR1, &sigact, NULL);

   /** 50128 */
   sigact.sa_handler = sigwinch;
   sigemptyset(&sigact.sa_mask);
   sigact.sa_flags = 0;
   sigaction(SIGWINCH, &sigact, NULL);

#ifdef SA_SIGINFO
   sigact.sa_handler = sigusr2;
   sigemptyset(&sigact.sa_mask);
   sigact.sa_flags = 0;
   sigaction(SIGUSR2, &sigact, NULL);
#endif

#endif

   g_p = p = calloc(1,sizeof(struct DIAL_PARMS));
   if (p==NULL) {
      kill_net_client();
      exit(EXIT_NO_MEM);
   }
   p->client = 1;
#ifdef NORMAL 
   if (argc<2) {
      arg_error_out("No Argument List\n"," "); 
   }

   for (arg = 1; arg < argc; arg++) {
      switch (argv[arg][0]) {
         case '-':
            switch (tolower(argv[arg][1])) {
               case 'b':
                  p->baud = atoi(&argv[arg][2]);
                  break;
               case 'c':
                  ctsexit = 1;
                  break;
               case 'd':
                  p->debug = atoi(&argv[arg][2]);
                  break;
               case 'l':
                  p->line = malloc(strlen(&argv[arg][2]));
                  if (p->line!=NULL) strcpy(p->line,&argv[arg][2]);
                  else arg_error_out("no memory on -l arg\n",argv[arg]);
                  break;
               case 'x':
                  p->path = malloc(strlen(&argv[arg][2]));
                  if (p->path!=NULL) strcpy(p->path,&argv[arg][2]);
                  else arg_error_out("no memory on -x arg\n",argv[arg]);
                  break;
               case 's':
                  p->script = malloc(strlen(&argv[arg][2]));
                  if (p->script!=NULL) strcpy(p->script,&argv[arg][2]);
                  else arg_error_out("no memory on -s arg\n",argv[arg]);
                  break;
               case 'k':
                  atexit(kill_net_client);
                  break;
               case 'i':
                  config_file = &argv[arg][2];
                  break;
               default:
                  arg_error_out("bad arg at:",argv[arg]);
                  break;
            }
            break;
         default:
            arg_error_out("bad arg at:",argv[arg]);
            break;
      }
   }

#else
   printf("********************* test version ***************\n");
   p->baud = 19200;
   p->debug = atoi(&argv[1][2]);
   p->phone = malloc(strlen("8339482"));
   strcpy(p->phone, "8339482");
   p->line = malloc(strlen("/dev/tty0p4"));
   strcpy(p->line, "/dev/term/s03");
   p->path = malloc(strlen("branch/0000"));
   strcpy(p->path, "branch/0000");
   p->script = malloc(strlen("setup.scr"));
   strcpy(p->script, "setup.scr");
   p->direction = 'o';
#endif

   GetPrivateProfileString("net_client", "path", prepath, sizeof(prepath),
      "/usr/hlcp", config_file);

   if (CreatePIDFile(prepath, "client", NULL) < 0) {
      error_out(p, strerror(errno));
      program_return_code = 12;
      exit(program_return_code);
   }

   if (p->debug>3) {
      fprintf(verbose,"debug: %d\n",p->debug);
      fprintf(verbose,"line: %s\n",p->line);
      fprintf(verbose,"xfer path: %s\n",p->path);
      fprintf(verbose,"script: %s\n",p->script);
      fprintf(verbose,"baud: %d\n",p->baud);
   }

   /* test the baud rate */
#ifndef DOS 
   switch(p->baud) {
      case 300:
      case 1200:
      case 2400:
      case 4800:
      case 9600:
      case 19200:
         break;
      default:
         error_out(p,"Bad baud rate value");
         exit(EXIT_BAD_PARM);
         break;
   }
#endif

/* Get the line */
   if (err=open_line(p)) {
      perror_out(p,"error on line open",err);
      program_return_code = EXIT_LINE_ERR;
      exit(program_return_code );
   }

   if (ctsexit) {
      if (!get_cts(p)) {
         error_out(p,"modem CTS is not asserted-- is modem turned on?");
         program_return_code = EXIT_NO_CTS;
         exit(program_return_code );
      }
   }
   else {
      if (!get_cts(p)) {
         error_out(p,"waiting for modem CTS -- is modem turned on?");
         while (!get_cts(p)) sleep(5);
         error_out(p,"CTS is now on.");
      }
   }

   while (1) {
      for(i=0;i<LINE_TRIES;i++) {
         dtr_down(p);
         sleep(1);
         dtr_up(p);
         sleep(1);
         flush_line(p);

         if (err=do_script(p,SCRIPT_INIT)) {
            if (err==GETLINE_TIMEOUT)
               error_out(p,"Timeout on script file");
            else if (err==BAD_SCRIPT)
               error_out(p,"bad script file");
            else
               perror_out(p,"error on init script",err);
         }
         else
            break;
      }

      if (err) {
         program_return_code = EXIT_LINE_ERR;
         exit(program_return_code );
      }

      do_one(p);   /* do one cycle.  */
      break;       /* we've decided to exit after one go, and let the script
                      restart us */
   }

   exit(program_return_code);
}

void do_one(struct DIAL_PARMS *p)
{
   char buf[256];
   int len;
   char *modem_response;
   char *tp;
   int flag;

   while (!p->connected) {
      wait_for_input(p);   /* hangs in a select until something arrives */
      if (p->debug>9) {
         printf("done waiting\n");
      }

      len = get_line_buf(p,buf,sizeof(buf)-1,4);

      if (p->debug>9) printf("len: %d",len);

      if (len==GETLINE_TIMEOUT) {
         if (p->debug>9) debug_out("partial input: ",buf);
         continue;
      }

      buf[len]='\0';
      if (p->debug>9) debug_out("Received: ",buf);
      fflush(stdout);

      if ( (modem_response = strstr(buf,"CONNECT"))!=NULL) {
         if ( (tp = strchr(modem_response,'\r'))!=NULL) *tp='\0';
         debug_out("call received: ",modem_response);
         p->connected = 1;
      }
      else if ( (modem_response = strstr(buf,"RING"))!=NULL) {
         if ((tp = strchr(modem_response,'\r'))!=NULL)
            *tp='\0';

         debug_out("ring received: ",modem_response);
         send_line(p,"ata\r",4);

         /* we send the ata manually to make sure there is no chance that
         the modem will answer unless this program is running properly */
      }
   }

   /* we now have a call */
   /* enter file transfer mode */
   rx_sm_init(p);
   tx_sm_init(p);
   set_nonblocking(p);

   nonblock_wait(p);   /* stay in there until we get a disconnect */

   /* transfer over, or line dropped */
   if ((p->rx_state==RX_DONE) && (p->tx_state==TX_DONE)) {
      if (p->txcrcerr) pinfo(p,"tx halted by error on other side\n");
      else pinfo(p,"normal end\n");
   }
   else {
      pinfo (p,"unexpected disconnect");
   }

   rx_sm_close(p);
   tx_sm_close(p);

   set_blocking(p);

   while ((len = get_line_buf(p,buf,sizeof(buf)-1,2)) !=GETLINE_TIMEOUT) {
      if (p->debug>8) {
         printf("len: %d",len);
         buf[len]='\0';
         printf("%s\n",buf);
      }
   }

   dtr_down(p);
   sleep(1);
   do_script(p,SCRIPT_EXIT);
}

void sigdeath()
{
   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   exit(1);
}

void sighup()
{
   printf("hangup!\n");
   fflush(stdout);

   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   exit(1);
}


int wait_for_input(struct DIAL_PARMS *p)
{
   int readfds;
   struct timeval timeout;
   struct timeval *timeoutp;
   int i;
   int k;
   int readmask;

   readmask = MASK(p->fdev);
   if (p->debug>11) printf("readmask=%08lx\n",readmask);

   timeout.tv_sec = 3; /* wait 3 secs */
   timeout.tv_usec=0;

   if (p->connected) timeoutp = &timeout;
   else timeoutp = NULL;

   while (1) {
      readfds = readmask;
      if (p->debug>9) printf("read=%08lx \n",readfds);
      i = select(p->fdev+1, &readfds,NULL,NULL,timeoutp);
      if (p->debug>9) printf("i=%d, read=%08lx \n",i,readfds);
      if ((p->debug>10) && (i<0)) printf("oops***********i=%d\n",i);
      if (!get_dcd(p)) {
         p->connected = 0;
      }

      if (i==0) return 0;
      else if (i>0) return 1; /* always go back if there is something to read */
   }
}

#ifdef SA_SIGINFO
void sigusr1(int signo, struct siginfo *pinfo)
{
   if (pinfo != NULL)
      kill( pinfo->si_pid, SIGUSR2);

   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   _exit(1);
}

void sigwinch(int signo, struct siginfo *pinfo)
{
   if (pinfo != NULL)
      kill( pinfo->si_pid, SIGUSR2);

   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   _exit(0);
}
#else
void sigusr1(int signo)
{
   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   _exit(1);
}

void sigwinch(int signo)
{
   if (g_p!=NULL) {
      dtr_down(g_p);
      if (g_p->flow)  {
         flow_off(g_p);
         g_p->flow = 0;
      }
   }

   _exit(0);
#endif

volatile sig_atomic_t signal_acked = 0;

void kill_net_client(void)
{
   char prepath[_POSIX_PATH_MAX];
   char nc_pidfile[_POSIX_PATH_MAX];
   pid_t net_pid;
   int attempt = 0;
   int signo;

   GetPrivateProfileString("net_client", "path", prepath, sizeof(prepath),
      "/usr/hlcp", config_file);
   sprintf(nc_pidfile, "%s/net_client.pid", prepath);

   if ((net_pid = ReadPIDFile(nc_pidfile)) < 2)
      return;

   signo = (program_return_code != 0) ? SIGUSR1 : SIGWINCH;

#ifdef SA_SIGINFO
   while (attempt < 3 && !signal_acked) {
      if (kill( net_pid, signo) < 0)
         return;

      if (!signal_acked)
         sleep(30);

      attempt++;
   }

   if (!signal_acked)
      kill( net_pid, SIGKILL);
#else
   kill( net_pid, signo);

#endif
}

#ifdef SA_SIGINFO
void sigusr2(int signo)
{
   signal_acked = 1;
}
#endif
