#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "common.h"

void wait_for_me(int signo);

int cc_status;
int signaled;

/*
 ****************************************************************************
 *
 *  Function:    call_cobol
 *
 *  Description: This function executes a program using the system() call.
 *               It was designed to execute runcbl; but, can be used to call
 *               any program.  It creates a datafile that will be used to
 *               simulate the "LINKAGE-AREA" of the COBOL program.
 *
 *  Parameters:  cmd      - The command line to pass to system() call.
 *               datafile - Name of datafile to pass to COBOL.
 *               buffer   - Buffer to write to & read from data file.
 *               bufsize  - Size of buffer in bytes.
 *
 *  Returns:     exit status of runcbl; -1 on error
 *
 ****************************************************************************
*/
int call_cobol(int s, char *cmd, char * argv[], char *datafile, void *buffer, size_t bufsize)
{
	int rv;
   int fd;
	fd_set fs;
	pid_t pid;
	sigset_t ss;
	sigset_t oset;
	struct sigaction old;
	struct timeval to = { 1, 0 };

	sigemptyset(&ss);
	sigaddset(&ss, SIGCHLD);
	sigprocmask(SIG_UNBLOCK, &ss, &oset);

	setsignal(SIGCHLD, wait_for_me, &old, SA_NOCLDSTOP, 0);

	FD_ZERO(&fs);
	FD_SET(s, &fs);

   /* create data file for memory contents */
   if ((fd = creat(datafile, READWRITE)) < 0)
      return -1;

   /* write to data file */
   if (writen(fd, (char *)buffer, bufsize) < 0)
      return -1;

	close(fd);

#if 0
   /* execute runcbl */
   if ((cc_status = system(cmd)) < 0)
      return -1;
#endif

	cc_status = 0;

	pid = fork();
	switch (pid) {
		case -1: /* forking error  */
			break;
		case 0:  /* child process  */
			execvp(cmd, argv);
			exit(1);
			break;
		default: /* parent process */
			while (!signaled) {
				rv = select(s + 1, &fs, NULL, NULL, &to);
				switch (rv) {
					case -1: /* error occurred */
						break;
					case 0:  /* timeout        */
						break;
					default: /* ready          */
						/* something came in on the socket, not good */
						kill(pid, SIGTERM);
				}
			}
	}

	sigaction(SIGCHLD, &old, NULL);

	if ((fd = open(datafile, O_RDONLY)) < 0)
		return -1;

   /* read from data file */
   if (readn(fd, (char *)buffer, bufsize) < bufsize)
      return -1;

	close(fd);

   return ((WIFEXITED(cc_status) && ! WIFSIGNALED(cc_status)) ? WEXITSTATUS(cc_status) : 
      -1);
}

void wait_for_me(int signo)
{
	signaled = 1;
	waitpid(-1, &cc_status, WNOHANG);
}
