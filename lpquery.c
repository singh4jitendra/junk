#include <curses.h>
#include <string.h>
#include <term.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/utsname.h>
#include <signal.h>

#define USAGE		"USAGE: %s [ -p printer ] [ -w user_name ] [ -f form ] \n        [ -s spool_id ] [ -c (no colors)]\n"
#define PRTPATH		"/etc/lp/printers"
#define CLASSPATH	"/etc/lp/classes"
#define ADMINPATH	"/usr/sbin"
#define LPPAGE		66
#define USRCNT		50

#ifndef _GNU_SOURCE
extern char* sys_errlist[], *optarg;
extern int errno, optind, opterr, optopt;
int getopt();

pid_t getpid();
#endif

char lptmpfile[20], command[80], command2[80];
FILE *fd, *fd1, *fd2, *fd0;
int olduid, pid, EXIT=0;
struct utsname *sysinfo;

char red[15], green[15], yellow[15], blue[15], repeat[15], rmso[15];
char LPID[25], WHO[25], SIZE[25], MONTH[25], DAY[25], TIME[25], STATUS[25];
char ptype[3], pvalue[256];
char line[256], params[80], SPPATH[20+SYS_NMLN];
char numcopies[5], desprt[15], formname[25], userfile[USRCNT][256], priority[3];
char issueuser[15], lpfile[80], pcline[256]; 
char *basename(), *ptr, cmdarg, specialcmds[80], *realstat;
char PRINTER[20], *ONLYWHO, *ONLYFORM, *ONLYSPID;
int i=0, numlines=0, pagecount[USRCNT], usercount=0;
void *abort();
int cnt=0, find_path=0,num_paths=0;
char strpaths [80][80];
char * stock_lp;

main (argc,argv)
int argc;
char **argv;
{

   signal(SIGHUP ,SIG_IGN);
   signal(SIGINT ,(void (*)(int))abort);
   signal(SIGQUIT,(void (*)(int))abort);
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

   sysinfo=(struct utsname *)malloc(sizeof (struct utsname));
   if ( (int)uname(sysinfo) < 0 ) {
      error_out("Unable to determine system node name.");
      EXIT=-1;
      cleanup();
   }
   sprintf(SPPATH,"/var/spool/lp/tmp");

   pid=getpid();
   sprintf(lptmpfile,"/tmp/lpstat-o.%d",pid);
   sprintf(command,"lpstat -o | grep -v \"^	\" %s | sort -b -k 4,4 >%s 2>&1",specialcmds,lptmpfile);
   system(command);

   sprintf(command2,"ls /var/spool/lp/tmp > /tmp/paths");
   system(command2);
   if ((fd0 = fopen("/tmp/paths","r")) == (FILE *) NULL){
	error_out("Unable to open paths file");
        EXIT=-1;
	cleanup();
   }
   while (fgets (line,256,fd0) != (char *)NULL){
	sscanf(line,"%s",strpaths[cnt]);
	cnt++;
  }
  num_paths=cnt;

   if ((fd = fopen(lptmpfile,"r")) == (FILE *)NULL) {
      error_out("Unable to open temporary status file.");
      EXIT=-1;
      cleanup();
   }

   while (fgets(line,256,fd) != (char *)NULL) {
      clear_fields();
      sscanf(line,"%s %s %s %s %s %s %s",LPID,WHO,SIZE,MONTH,DAY,TIME,STATUS);

      if (ONLYSPID && (strncmp(LPID,ONLYSPID,strlen(LPID)) != 0)) 
	continue;
      ptr=(char *)strrchr(LPID,'-');
      ptr++;
      find_path=0;
      cnt=0;
      while ((find_path != 1)&&(cnt <= num_paths)) {
       sprintf(lpfile,"%s/%s/%s-0",SPPATH,strpaths[cnt],ptr);
       if ((fd1 = fopen(lpfile,"r")) != (FILE *)NULL) 
	{
         find_path=1;
        }
       cnt++;
      }
  
      while (fgets(params,80,fd1) != (char *)NULL) {
         sscanf(params,"%s %s",ptype,pvalue);
         switch (ptype[0]) {
	    case 'C':
			strcpy(numcopies,pvalue);
			break;
	    case 'D':
			strcpy(desprt,pvalue);
			strncpy(formname,pvalue,5);
			formname[5]='\0';
			break;
	    case 'F':
			strcpy(userfile[usercount],pvalue);
			if (access(userfile[usercount],(int)F_OK) != (int)0) 
			   strcpy(userfile[usercount],"/dev/null");
			usercount++;
			break;
	    case 'P':
			strcpy(priority,pvalue);
			break;
	    case 'U':
			strcpy(issueuser,pvalue);
			break;
	    case 'H':
			if (realstat) free((char *)realstat);
			realstat=(char *)malloc(strlen(pvalue)+5);
			strcpy(realstat,pvalue);
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
  
      if (fd2) fclose((FILE *)fd2);
   }

      if (strlen(desprt) > 4) {
	ptr=desprt+5;
	strcpy(desprt,ptr);
      }

      if (strncmp(desprt,formname,5) == 0) 
		stock_lp = getenv("STOCK_LP");
		if (stock_lp == NULL) stock_lp = "STOCK";
	    strcpy(formname,stock_lp);

      if (ONLYFORM && (strncmp(formname,ONLYFORM,strlen(formname)) != 0))
	 continue;

  for (i=0; i < usercount; i++) {
      putp(blue); printf(" %-16s ",LPID); putp(rmso); 
      if ( usercount > 1 ) putp(repeat);
      printf("%-15s",basename(userfile[i])); putp(rmso);
      printf("%-10s",issueuser);
      putp(blue); printf("%-8s",desprt); putp(rmso);
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
		putp(yellow);
		if (realstat) 
		   printf("%-9s",realstat);
		else 
		   printf("%-9s","wait"); 
		putp(rmso);
	        printf("\n");
		break;
      } 
}
   }
   if (fd) fclose((FILE *)fd);
   
   cleanup();
}

clear_fields() {
      int i=0;

      LPID[0]=		'\0';
      WHO[0]=		'\0';
      SIZE[0]=		'\0';
      MONTH[0]=		'\0';
      DAY[0]=		'\0';
      TIME[0]=		'\0';
      STATUS[0]=	'\0';
      numcopies[0]=	'\0';
      desprt[0]=	'\0';
      formname[0]=	'\0';
      priority[0]=	'\0';
      issueuser[0]=	'\0';
      numlines=		0;
      usercount=	0;
      for (i=0; i < USRCNT; i++) {
         pagecount[i]=		1;
         userfile[i][0]=		'\0';
      }
      if (realstat) free((char *)realstat); realstat=(char *)NULL;
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
    EXIT=2;
    cleanup();
    return((void *)EXIT);     /* line does nothing but satisfy compiler complaints */
}

cleanup() {
   if (fd) 	 fclose((FILE *)fd);
   if (fd1) 	 fclose((FILE *)fd1);
   if (fd2) 	 fclose((FILE *)fd2);
   if (fd0)      fclose((FILE *)fd0);
   if (sysinfo)  free((struct utsname *)sysinfo);
   if (ONLYWHO)  free((char *)ONLYWHO);
   if (ONLYFORM) free((char *)ONLYFORM);
   if (ONLYSPID) free((char *)ONLYSPID);
   if (realstat) free((char *)realstat);
   sprintf(command,"rm -f %s >/dev/null 2>&1",lptmpfile);
   system(command);
   setuid(olduid);
   reset_shell_mode();
   exit(EXIT);
}

error_out(message)
char *message;
{
   printf("ERROR: errno %d - %s\n",errno,message);
#ifdef _GNU_SOURCE
   printf("%s\n",strerror(errno));
#else
   printf("%s\n",*(sys_errlist+errno));
#endif
}

init_colors() {
   def_shell_mode();
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
