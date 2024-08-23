#include <stdio.h>
#include <sys/signal.h>
#include <string.h>
#include <termio.h>
#include <time.h>
extern int errno;

main(argc,argv)
char *argv[];
int argc;
{
char *crypt();
char passwd[20],*epasswd;
struct tm *currday;
long currtime;

if (argc > 1) {
  strcpy(passwd,argv[1]);
  epasswd = crypt(passwd,"W2");
  printf("passwd = %s,  encrypted = %s\n",passwd,epasswd);
}
currtime = time((long *) 0);
currday = gmtime(&currtime);
printf("year = %d,day of  year = %d",currday->tm_year,currday->tm_yday);
printf("  combined = %d%d\n",currday->tm_year,currday->tm_yday);
}
