/*  superx.c - This program is used to change user id to root and execute   */
/*             the rest of the command line passed to this module.          */

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

main(argc,argv)
int argc;
char *argv[];
{
int olduid,i;
char buff[256];
buff[0] = 0; 
if (argc < 2)  exit(0);
olduid=geteuid();
if (setuid(0) == -1) exit(1);
if (argc == 2) strcpy(buff,argv[1]);
else
   for (i=1;i<argc;i++) sprintf(buff,"%s %s",buff,argv[i]);
if (system(buff) == -1) exit(2);
setuid(olduid);
exit(0);
}
