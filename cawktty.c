#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <utmp.h>
#include <libgen.h>

#define NUMPATTERNS		25

extern char *__loc1;
struct utmp *utentry;
char username[20];
pid_t cursid;
char ttydevice[80];
char *gettty(), *console, *diagport;
char *ptr1, *ptr2, matchval[10], matchval2[10];
int i;
char patterns[NUMPATTERNS][80] = { "/dev/term/s([0-9].*)$0$",
				   "/dev/term/([0-9].*)$0$",
				   "/dev/tty([0-9].*)$0p([0-9].*)$1$",
				   "/dev/tty([0-9ab].*)$0$",
				   "/dev/vt0([0-9])$0$",
			 	   "" };

main (argc,argv)
int argc;
char **argv;
{
    if (argc <= 1) sprintf(ttydevice,"/dev/%s\n",gettty());
    else strcpy(ttydevice,argv[1]);

    console=(char *)getenv("CONSOLE");
    diagport=(char *)getenv("DIAGPORT");

    if (strncmp(console,ttydevice,strlen(console)) == 0) {
       printf("b\n");
       exit(0);
    }
    if (strncmp(diagport,ttydevice,strlen(diagport)) == 0) {
       printf("a\n");
       exit(0);
    }

    i=0;
    while (*patterns[i] != (char *)NULL) {
       ptr1 = regcmp(patterns[i],(char *)0);
       ptr2 = regex(ptr1,ttydevice,matchval,matchval2);
       if (strncmp(ttydevice,__loc1,strlen(ttydevice)) == 0) break;
       i++; matchval[0]='\0'; matchval2[0]='\0';
     }
    

    switch(i) {
	case 0:
	case 1:
	case 3:
		printf("%s\n",matchval);
		break;
	case 2:
		printf("%d\n",(atoi(matchval)*16)+(atoi(matchval2)));
		break;
	case 4:
		printf("V%s\n",matchval);
		break;
	default:
		printf("X%s\n",ttydevice+strlen(ttydevice)-1);
		break;
    }

    if (ptr1) free((char *)ptr1);
    if (ptr2) free((char *)ptr2);
}

char *gettty() {
    strcpy(username,getlogin());
    cursid=getsid(getpid());
    while (cursid != utentry->ut_pid) 
	utentry=getutent();
    return(utentry->ut_line);
}
