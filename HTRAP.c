/*  HTRAP.c -  This program is used to trap a hangup signal and execute the */
/*             command passed to it.                                        */

#include <sys/signal.h>
#include <string.h>
static char htrapbuff[81];

int HTRAP(hgpcmd)
char *hgpcmd;
{
int htrapcmd();
strncpy(htrapbuff,hgpcmd,80);
htrapbuff[80] = 0;
signal(SIGHUP,(void(*)(int))htrapcmd);
return(0);
}

int htrapcmd(signum) 
int signum;
{
system(htrapbuff);
exit(0);
}
