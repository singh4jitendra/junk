#include <curses.h>
#include <string.h>
#include <term.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>

#define USAGE 		"USAGE: %s [ -p printer ] [ -w user_name ] [ -f form ] \n        [ -s spool_id ] [ -c (no colors)]\n"
#define SPPATH		"/usr/spool/lp/request"
#define PRTPATH		"/usr/spool/lp/member"
#define CLASSPATH	"/usr/spool/lp/classes"
#define ADMINPATH	"/usr/lib"
#define LPPAGE		66
#define USRCNT		50

extern char* sys_errlist[], *optarg;
extern int errno, optind, opterr, optopt;
int getopt();

pid_t getpid();
char lptmpfile[20], command[80];
FILE *fd, *fd1, *fd2;
int olduid, pid, EXIT=0;

char red[15], green[15], yellow[15], blue[15], rmso[15], repeat[15];
char LPID[25], WHO[25], PRIORITY[10], PRIOVAL[5], MONTH[25], DAY[25], TIME[25], STATUS[25];
char ptype[256], LPIDfile[25], *pvalue;
char line[256], params[80], pipefile[256];
char numcopies[5], desprt[15], formname[25], userfile[USRCNT][256], priority[3];
char issueuser[15], lpfile[80], srchfile[80], pcline[256]; 
char *basename(), *ptr, cmdarg, specialcmds[80];
char PRINTER[20], *ONLYWHO, *ONLYFORM, *ONLYSPID;
int i=0, numlines=0, pagecount[USRCNT], usercount=0;
void *abort();

main (argc,argv)
int argc;
char **argv;
{
   signal(SIGHUP ,SIG_IGN);
   signal(SIGINT ,abort);
   signal(SIGQUIT,abort);
   signal(SIGKILL,SIG_IGN);
   signal(SIGPIPE,SIG_IGN);
   signal(SIGALRM,SIG_IGN);
   signal(SIGTERM,SIG_IGN);
   signal(SIGUSR1,SIG_IGN);
   signal(SIGUSR2,SIG_IGN);
   signal(SIGSTOP,SIG_IGN);

   olduid=geteuid();
   if (setuid(0) < 0) {
	error_out("Unable to set super user id.\n");
	EXIT=-1;
	cleanup();
   }
   init_colors();
   for (i=0; i<USRCNT; i++) pagecount[i]=1;
   
   strcpy(specialcmds,"");
   while ((cmdarg=getopt(argc,argv,"p:w:cf:s:")) != (int)EOF)
      switch(cmdarg) {
	case 'p':
		strcpy(PRINTER,optarg);
		sprintf(specialcmds,"%s | grep %s ",specialcmds,optarg);
		break;
	case 'w':
		ONLYWHO=(char *)malloc(strlen(optarg)+5);
		strcpy(ONLYWHO,optarg);
		break;
	case 'c':
		revoke_colors();
		break;
	case 'f':
		ONLYFORM=(char *)malloc(strlen(optarg)+5);
		strcpy(ONLYFORM,optarg);
		break;
	case 's':
		ONLYSPID=(char *)malloc(strlen(optarg)+5);
		strcpy(ONLYSPID,optarg);
		break;
	case '?':
		fprintf(stderr,USAGE,argv[0]);
		EXIT=2;
		cleanup();
      }
		

   pid=getpid();
   sprintf(lptmpfile,"/tmp/lpstat-o.%d",pid);
   sprintf(command,"lpstat -o | grep -v \"^	\" %s | sort +3b -4b >%s 2>&1",specialcmds,lptmpfile);
   system(command);

   if ((fd = fopen(lptmpfile,"r")) == (FILE *)NULL) {
      error_out("Unable to open temporary status file.");
      EXIT=-1;
      cleanup();
   }

   while (fgets(line,256,fd) != (char *)NULL) {
      clear_fields();
      sscanf(line,"%s %s %s %s %s %s %s %s",LPID,WHO,PRIORITY,PRIOVAL,MONTH,DAY,TIME,STATUS);

      if (ONLYSPID && (strncmp(LPID,ONLYSPID,strlen(LPID)) != 0))  
	continue;
      strcpy(LPIDfile,LPID);
      ptr=(char *)strrchr(LPID,'-');
      *ptr='\0';
      sprintf(srchfile,"/tmp/lpsrch.%d",pid);
      sprintf(command,"grep -l J%s-%s %s/%s/c* >%s 2>/dev/null",LPID,ptr+1,SPPATH,LPID,srchfile);
      system(command);

      if ((fd1 = fopen(srchfile,"r")) == (FILE *)NULL) {
         error_out("Unable to open LP configuration file - %s\n.",srchfile);
         EXIT=-1;
	 cleanup();
      }
      if (fgets(lpfile,80,fd1) == (char *)NULL) {
	 error_out("Unable to determine LP configuration file.");
	 EXIT=-1;
	 cleanup();
      }
      if (fd1) fclose((FILE *)fd1);

      if (lpfile[strlen(lpfile)-1] == '\n') lpfile[strlen(lpfile)-1] = '\0';
      if ((fd1 = fopen(lpfile,"r")) == (FILE *)NULL) {
         error_out("Unable to open LP configuration file.");
         EXIT=-1;
	 cleanup();
      }
  
      while (fgets(params,80,fd1) != (char *)NULL) {
         sscanf(params,"%s",ptype);
	 pvalue=ptype+1;
         switch (ptype[0]) {
	    case 'K':
			strcpy(numcopies,pvalue);
			break;
	    case 'J':
			ptr=(char *)strrchr(pvalue,'-'); *ptr='\0';
			strcpy(desprt,pvalue);
			strncpy(formname,pvalue,5);
			formname[5]='\0';
			break;
	    case 'F':
			sprintf(pipefile,"%s/%s/%s",SPPATH,desprt,pvalue);
			break;
	    case 'N':
			strcpy(userfile[usercount],pvalue);
			if (strlen(pvalue) == 0)
			   strcpy(userfile[usercount],pipefile);
			if (access(userfile[usercount],(int)F_OK) != (int)0) 
			   strcpy(userfile[usercount],"/dev/null");
			usercount++;
			break;
	    case 'A':
			strcpy(priority,pvalue);
			break;
	    case 'L':
			strcpy(issueuser,pvalue);
			break;
	}
      }
      if (fd1) fclose((FILE *)fd1);
      if (ONLYWHO && (strncmp(issueuser,ONLYWHO,strlen(issueuser)) != 0))
	continue;

    for (i=0; i < usercount; i++) {
      if (strcmp(userfile[i],"/dev/null") != 0) {
        if ((fd2 = fopen(userfile[i],"r")) == (FILE *)NULL) {
           error_out("Unable to open print file.");
           EXIT=-1;
	   cleanup();
        }

	numlines=0;
        while (fgets(pcline,256,fd2) != (char *)NULL) {
  	   numlines++; ptr=pcline;
	   while ((ptr=(char *)strchr(ptr,'\f')) != (char *)NULL)
	      { pagecount[i]++; numlines=0; ptr++; }
	   if ( numlines >= LPPAGE )
	      { pagecount[i]++; numlines=0; }
        }
        pagecount[i] *= (int)atoi(numcopies);
        if (fd2) fclose((FILE *)fd2);
      }
      else { pagecount[i]=0; } 

      if (strncmp(SPPATH,userfile[i],(int)strlen(SPPATH)) == 0) 
	 strcpy(userfile[i],"Data_Piped_In");

      if (fd2) fclose(fd2);
    }


      if (strlen(desprt) > 4 ) {
 	 ptr=desprt+5;
	 strcpy(desprt,ptr);
      }

      if (strncmp(formname,desprt,5) == 0)
	 strcpy(formname,"00000");

      if (ONLYFORM && (strncmp(formname,ONLYFORM,strlen(formname)) != 0)) 
	continue;


    for (i=0; i < usercount; i++) {
      putp(blue); printf("%-14s",LPIDfile); putp(rmso);
      if ( usercount > 1) putp(repeat);
      printf("%-15s",basename(userfile[i])); putp(rmso);
      printf("%-10s",issueuser);
      putp(blue); printf("%-10s",desprt); putp(rmso);
      printf("%-4s%-6d",numcopies,pagecount[i]);
      printf("%-6s%-3s",formname,priority);

      switch (STATUS[0]) {
	  case 'o':
		putp(green); printf("%-9s","printing"); putp(rmso);
		printf("\n");
		break;
	  case 'b':
	  case 'h':
		putp(red); printf("%-9s","holding"); putp(rmso);
		printf("\n");
		break;
	  case 'c':
		putp(red); printf("%-9s","canceled"); putp(rmso);
		printf("\n");
		break;
	  default: 
		putp(yellow); printf("%-9s","wait"); putp(rmso);
		printf("\n");
		break;
      } 
    }
   }
   
   cleanup();
}

clear_fields() {
      int i=0;

      LPID[0]=		'\0';
      WHO[0]=		'\0';
      PRIORITY[0]=	'\0';
      PRIOVAL[0]=	'\0';
      MONTH[0]=		'\0';
      DAY[0]=		'\0';
      TIME[0]=		'\0';
      STATUS[0]=	'\0';
      numcopies[0]=	'\0';
      desprt[0]=	'\0';
      formname[0]=	'\0';
      priority[0]=	'\0';
      pipefile[0]=	'\0';
      numlines=		0;
      usercount=	0;
      issueuser[0]=	'\0';
      for (i=0; i < USRCNT; i++) {
         pagecount[i]=		1;
         userfile[i][0]=	'\0';
      }
}

char *basename(string)
char *string;
{
   char *ptr;
   if ((ptr=(char *)strrchr(string,'/')) != (char *)NULL) { ptr++; return(ptr); }
   return(string);
}

void *abort(sig)
int sig;
{
   signal(sig,SIG_IGN);
   cleanup();
}

cleanup() {
   if (fd) 	 fclose((FILE *)fd);
   if (fd1) 	 fclose((FILE *)fd1);
   if (fd2) 	 fclose((FILE *)fd2);
   if (ONLYWHO)	 free((char *)ONLYWHO);
   if (ONLYFORM) free((char *)ONLYFORM);
   if (ONLYSPID) free((char *)ONLYSPID);
   sprintf(command,"rm -f %s %s >/dev/null 2>&1",lptmpfile,srchfile);
   system(command);
   setuid(olduid);
   reset_shell_mode();
   exit(EXIT);
}

error_out(message)
char *message;
{
   printf("ERROR: errno %d - %s\n",errno,message);
   printf("%s\n",*(sys_errlist+errno));
}

init_colors() {
   def_shell_mode();
/* Until HP becomes color capable (terminfo) disable the following lines
   setupterm((char *)0,1,(int *)0);
   if ( magic_cookie_glitch <= 0 && max_colors > 0 ) {
     sprintf(red,"%s",tparm(tigetstr("setaf"),1));
     sprintf(green,"%s",tparm(tigetstr("setaf"),2));
     sprintf(yellow,"%s",tparm(tigetstr("setaf"),3));
     sprintf(blue,"%s",tparm(tigetstr("setaf"),6));
     sprintf(repeat,"%s",tparm(tigetstr("setaf"),5));
     sprintf(rmso,"%s",tparm(tigetstr("rmso")));
   }
   else 
*/
     revoke_colors();
}

revoke_colors() {
   sprintf(red,"");
   sprintf(green,"");
   sprintf(yellow,"");
   sprintf(blue,"");
   sprintf(repeat,"");
   sprintf(rmso,"");
}
