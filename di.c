#include "common.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

/*
 ******************************************************************************
 *
 * Function:    daemoninit
 *
 * Description: This function forks the process to disassociate from the
 *              controlling terminal.  The parent process terminates; the
 *              child process continues as the daemon.  The only process
 *              to return from this function is the child process.
 *
 * Parameters:  progName - Pointer to name of program.
 *
 * Returns:     nothing
 *
 ******************************************************************************
*/
void daemoninit(char *progName)
{
    switch (fork()) {
        case -1:    /* forking error */
            wescolog(NETWORKLOG, "%s: unable to fork daemon.", progName);
            err_sys("error forking daemon");
            break;
        case 0:    /* child process */
            setsid();
            chdir("/");
            break;
        default:  /* parent process */
            exit(0);
    }
}
