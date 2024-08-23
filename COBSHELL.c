/*  This subroutine is used to execute a subshell passing it the arguments
    that we received.  It is primarily called from cobol but could be
    called from C also.  The arguments are used as follows:

    shcmd       char ptr        This is the actual shell command that is to be
                                executed by the subshell.

    hgpflag     char ptr        This is a one byte field that signifies whether
                                we should trap a hangup signal.

    waitflag    char ptr        This is a one byte field that signifies whether
                                we should return control immediately or wait 
                                until the subshell has completed.
 
 
    Author   -  Robert Wood

    Date     -  09/21/89
 
*/

#include <sys/signal.h>
#include <string.h>
static char gothgp;

int COBSHELL(shcmd,hgpflag,waitflag)
char *shcmd,*hgpflag,*waitflag;
{
char buff[100];
int cbshhgp();

/*  Lets initialize what we need   */

gothgp = 'n';   

/*  Should we trap a hangup ?  if so set up the trap.    */

if (hgpflag[0] == 'Y') {
   signal(SIGHUP,(void(*)(int))cbshhgp);
   strcpy(buff,"nohup ");
   strncat(buff,shcmd,80);
}

/*  Fire up the sub shell   */

system(buff);         

/*  The following loop waits for completion of the subshell if the caller
    requested such an action.  If the hangup flag was set, we will continue
    to wait for a completion even if the user hangs up.   */

wait_again:
if (waitflag[0] == 'Y') wait((int *) 0);
if ((hgpflag[0] == 'Y') && (gothgp == 'y')) {
   gothgp = 'X';
   goto wait_again;
}
else if (gothgp == 'X') exit(0);

/*  Lets set the signal handling back to normal and return  */

if (hgpflag[0] == 'Y') signal(SIGHUP,SIG_DFL);
return(0);
}
 
/*  The following routine is used by COBSHELL to trap a hangup signal  */

int cbshhgp(hgcode)
int hgcode;
{
gothgp = 'y';
return(0);
}
