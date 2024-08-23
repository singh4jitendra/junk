#include <stdio.h>
#include <string.h>
#include <errno.h>
void pgcall (cmd_file,rstat)
char *cmd_file, *rstat;

/* execute "program" in pgm and place exit code returned into "rstat" for use
by the calling program. The value retuned will be a four byte
null terninated string. Byte 1-3 will be a numeric, byte 4 a null. */
{
char *_ID =  "@(#)pgcall.c	1.3";
int TMODE = 05;
int SMODE = 777;
int n ;
char *strcat(), *strncpy();

/* check the permissions on the cmd_file */

	if(access(cmd_file,TMODE) != 0){
/* dont have read and execute permission */
/* change it if I can */

		if((n=chmod(cmd_file,SMODE)) != 0 ){
/* cant change it - bail out */

			sprintf(rstat,"%.3d",errno);
        		return;
		}
/* OK - run command */

	}
	n=(system(cmd_file));
	n=(n >> 8);
	sprintf(rstat,"%.3d",n);
        return;
}
