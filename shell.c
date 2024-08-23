/*
   Changed: 12/22/04 mjb - Project 04041. Added conditional compilations to
                           include errno.h if _GNU_SOURCE is defined and to
                           define CNUL if it is not defined.
*/

#include <string.h>
#include <stdio.h>
#include <termio.h>

#ifdef _GNU_SOURCE
#include <errno.h>
#else
extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;
#endif

#ifndef CNUL
#define CNUL 0
#endif

static char d_VERSION[] = "shell.c: 1.6 7/9/91";
char rstat[5];
struct termio d_oldtty, d_newtty;
extern unsigned alarm(), sleep();


SHELL(string,retcode)
char *string, *retcode;

{
int status;

	if (ioctl(0,TCGETA,&d_oldtty)<0){
		sprintf(rstat,"9100");
		strncpy(retcode,rstat,(strlen(retcode)));
		return(0);	
	}

	d_newtty.c_iflag = (BRKINT+IGNPAR+ISTRIP+ICRNL+IXON);
	d_newtty.c_oflag = (OPOST+ONLCR+TAB3);
	d_newtty.c_cflag = d_oldtty.c_cflag;
	d_newtty.c_lflag = (ISIG+ICANON+ECHO+ECHOK); 
	d_newtty.c_cc[0] = CINTR;
	d_newtty.c_cc[1] = CQUIT;
	d_newtty.c_cc[2] = CERASE;
	d_newtty.c_cc[3] = CKILL;
	d_newtty.c_cc[4] = CEOF; 
	d_newtty.c_cc[5] = CNUL; 
	
	if (ioctl(0,TCSETA,&d_newtty)<0){
		sprintf(rstat,"9200");
		strncpy(retcode,rstat,(strlen(retcode)));
		return(0);	
	}
	if ((status=system(string)) != 0 ){
		sprintf(rstat,"%04d",status);
		if (ioctl(0,TCSETA,&d_oldtty)<0){
			fprintf(stderr," error resetting CRT \n");
			sprintf(rstat,"9300");
		}
		strncpy(retcode,rstat,4);
		return(0);	
	}
	sprintf(rstat,"0000");
	if (ioctl(0,TCSETA,&d_oldtty)<0){
		fprintf(stderr," error resetting CRT \n");
		sprintf(rstat,"9300");
	}
	strncpy(retcode,rstat,(strlen(retcode)));
	return(0);	
}
