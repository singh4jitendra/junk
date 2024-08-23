#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#ifndef _GNU_SOURCE
#include <stropts.h>
#endif

#if defined(__hpux)
#	include <termios.h>
#elif defined(_POSIX_SOURCE) && defined(_XOPEN_SOURCE) 
#	undef _POSIX_SOURCE
#	undef _XOPEN_SOURCE

#ifdef _GNU_SOURCE
#include <pty.h>
#else
#  include <termios.h>
#endif

#	define _POSIX_SOURCE
#	define _XOPEN_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#ifdef _GNU_SOURCE
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#else
#include <macros.h>
#endif


#include "termcmds.h"
#include "err.h"

/* UNIX kernel functions */
int grantpt(int);
int unlockpt(int);
char *ptsname(int);

struct termcmds term_commands[4] = {
	{ TC_7902_4902_TERM, NCR_7902_CLEAR, NCR_7902_RMSO },
	{ TC_VT100_TERM, VT100_CLEAR, VT100_RMSO },
	{ TC_ADDS_TERM, ADDS_CLEAR, ADDS_RMSO },
	{ TC_AT386_TERM, AT386_CLEAR, AT386_RMSO }
};

volatile int pty_slave_child_died = 0;

void pty_sigchld_handler(int signo) { pty_slave_child_died = 1; }

/*
 ******************************************************************************
 *
 * Name:        ptym_open
 *
 * Description: This function opens the master terminal.
 *
 * Parameters:  pts_name - Name of the slave terminal.
 *
 * Returns:      0 - successful
 *              -1 - failed to open master terminal
 *              -2 - failed to change permissions for terminal
 *              -3 - failed to unlock terminal
 *              -4 - failed to get the slave name
 *
 ******************************************************************************
*/
int ptym_open(char *pts_name)
{
  char *ptr;
  int fdm;

  strcpy(pts_name, "/dev/ptmx");
  if ((fdm = open(pts_name, O_RDWR)) < 0)
    return -1;

  if (grantpt(fdm) < 0) {
    close(fdm);
    return -2;
  }

  if (unlockpt(fdm) < 0) {
    close(fdm);
    return -3;
  }

  if ((ptr = ptsname(fdm)) == NULL) {
    close(fdm);
    return -4;
  }

  strcpy(pts_name, ptr);
  return fdm;
}

/*
 ******************************************************************************
 *
 * Name:        ptys_open
 *
 * Description: This function opens the slave terminal.
 *
 * Parameters:  pts_name - Name of the slave terminal.
 *
 * Returns:      0 - successful
 *              -5 - failed to open slave terminal
 *              -6 - failed to push "ptem" onto slave
 *              -7 - failed to push "ldterm" onto slave 
 *              -8 - failed to push "ttcompat" onto slave
 *
 ******************************************************************************
*/
int ptys_open(int fdm, char *pts_name)
{
  int fds;

  if ((fds = open(pts_name, O_RDWR)) < 0) {
    close(fdm);
    return -5;
  }

#if !defined(_GNU_SOURCE)
  if (ioctl(fds, I_PUSH, "ptem") < 0) {
    close(fdm);
    close(fds);
    return -6;
  }

  if (ioctl(fds, I_PUSH, "ldterm") < 0) {
    close(fdm);
    close(fds);
    return -7;
  }

#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE)
  if (ioctl(fds, I_PUSH, "ttcompat") < 0) {
    close(fdm);
    close(fds);
    return -8;
  }
#endif

#endif

  return fds;
}

/*
 ******************************************************************************
 *
 * Name:        pty_fork
 *
 * Description: This function gets a pseudo-terminal and then forks the
 *              process.  The parent has access to the master; the child
 *              has access to the slave.
 *
 * Parameters:  ptrfdm        - Pointer to store the file descriptor for the
 *                              master.
 *              slave_name    - Name of the slave terminal.
 *              slave_termios - A termios structure used to set the attributes
 *                              of the slave terminal.
 *              slave_winsize - A winsize structure used to set the window
 *                              size of the slave terminal.
 *
 * Returns:      0 if parent, process id if child, -1 on error
 *
 ******************************************************************************
*/
pid_t pty_fork(int *ptrfdm, char *slave_name,
               struct termios *slave_termios,
               struct winsize *slave_winsize)
{
  int fdm, fds;
  pid_t pid;
  char pts_name[20];

  if ((fdm = ptym_open(pts_name)) < 0)
    err_sys("can't open master pty: %s %d", pts_name, fdm);

  if (slave_name != NULL)
    strcpy(slave_name, pts_name);

  if ((pid = fork()) < 0)
    return -1;
  else if (pid == 0) {    /* child */
    if (setsid() < 0)
      err_sys("setsid error");

    if ((fds = ptys_open(fdm, pts_name)) < 0)
      err_sys("can't open slave pty");

#if defined(TIOCSCTTY) && !defined(CIBAUD)
    if (ioctl(fds, TIOCSCTTY, (char *)0) < 0)
      err_sys("TIOCSCTTY error");
#endif

    if (slave_termios != NULL) {
      if (tcsetattr(fds, TCSANOW, slave_termios) < 0)
        err_sys("tcsetattr error on slave pty");
    }

    if (slave_winsize != NULL) {
      if (ioctl(fds, TIOCSWINSZ, slave_winsize) < 0)
        err_sys("TIOCSWINSZ error on slave pty");
    }

    close(STDIN_FILENO);
    if (dup2(fds, STDIN_FILENO) != STDIN_FILENO)
      err_sys("dup2 error to stdin");
    close(STDOUT_FILENO);
    if (dup2(fds, STDOUT_FILENO) != STDOUT_FILENO)
      err_sys("dup2 error to stdout");
    close(STDERR_FILENO);
    if (dup2(fds, STDERR_FILENO) != STDERR_FILENO)
      err_sys("dup2 error to stderr");

    if (fds > STDERR_FILENO)
      close(fds);

    close(fdm);
    return 0;
  }
  else {
    *ptrfdm = fdm;
    return pid;
  }
}

/*
 ******************************************************************************
 *
 * Name:        pty_server_io
 *
 * Description: This function redirects I/O from a socket descriptor to the
 *              master pty.  The slave pty sees data written to the master
 *              as standard input.  The slave writes to the master as
 *              standard output and standard error.  (50128)
 *
 * Parameters:  s      - The file descriptor for the socket.
 *              master - The file descriptor for the master pty.
 *
 * Returns:     0 if successful, -1 on error
 * 
 * History:     08/03/05 mjb 15056 - Moved the logic for setting the s_timeout
 *                                   structure into the infinite loop.  This
 *                                   ensures the timeout value is properly
 *                                   set when the "select" call is made.  This
 *                                   issue appeared with the migration to
 *                                   Linux.
 *
 ******************************************************************************
*/
int pty_server_io(int s, int ms, int master, int to_min, int term_index, int hb)
{
   int maxfd, num_bytes;
   int fds_ready;
   volatile int finished = 0;
   register int pty_bytes, fd_bytes;
   char pty_buffer[1024], fd_buffer[1024];
   char *p_pty_buffer, *p_fd_buffer;
   fd_set inmask, outmask, errmask, *p_outmask;
	struct timeval *p_timeout = (struct timeval *)NULL;
   struct timeval s_timeout = {0, 0};
	struct sigaction old_action;
	struct sigaction new_action;

	setsignal(SIGCHLD, pty_sigchld_handler, &old_action,
		SA_NOCLDSTOP, 1, SIGHUP);

   pty_bytes = fd_bytes = 0;
   maxfd = max(s, master) + 1;

   for ( ; ; ) {
      if (to_min > 0) {
         s_timeout.tv_sec = to_min * 60;
         p_timeout = &s_timeout;
      }
      else {
         p_timeout = (struct timeval *)NULL;
      }

      if ((finished) || (pty_slave_child_died)) {
			sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);
         return 0;
		}

      FD_ZERO(&inmask);
      FD_ZERO(&outmask);
      FD_ZERO(&errmask);

      p_outmask = (fd_set *)NULL;

      if (fd_bytes > 0) {
         FD_SET(master, &outmask);
         p_outmask = &outmask;
      }
      else
         FD_SET(s, &inmask);

      if (pty_bytes > 0) {
         FD_SET(s, &outmask);
         p_outmask = &outmask;
      }
      else
         FD_SET(master, &inmask);

		FD_SET(ms, &inmask);
      FD_SET(master, &errmask);

      fds_ready = select(maxfd, &inmask, p_outmask, &errmask, p_timeout);
      if (fds_ready < 0) {
         if (errno == EINTR)
            continue;

			sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);

         return -1;
      }
      else if (fds_ready == 0) {
			if (hb != 0) {
				gs_send_heartbeat(ms);
			}
			else {
				/* these suckers aren't doing anything, kick them off */
				write(s, term_commands[term_index].tc_clear,
					strlen(term_commands[term_index].tc_clear));
				write(s, term_commands[term_index].tc_rmso,
					strlen(term_commands[term_index].tc_rmso));
				sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);
	         return -1;
			}
		}

		/* data received on the main socket */
		if (FD_ISSET(ms, &inmask)) {
			gs_inspect_message(ms);
		}

      /* check to see if the socket is ready to read */
      if (FD_ISSET(s, &inmask)) {
         fd_bytes = read(s, fd_buffer, sizeof(fd_buffer));
         if (fd_bytes < 0) {
            if (errno == EWOULDBLOCK) {
               fd_bytes = 0;
               errno = 0;
            }
            else {
					sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);
               return -1;
				}
         }
         else if (fd_bytes == 0)
            finished = 1;
      }

      /* check to see if the pty is ready for a write */
      if ((fd_bytes > 0) && (FD_ISSET(master, &outmask))) {
         p_fd_buffer = fd_buffer;
         while (fd_bytes > 0) {
            num_bytes = write(master, p_fd_buffer, fd_bytes);
            if (num_bytes > 0) {
               fd_bytes -= num_bytes;
               p_fd_buffer += num_bytes;
            }
            else if (num_bytes <= 0) {
               if (errno != EWOULDBLOCK) {
						sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);
                  return 1;
					}
               else
                  errno = 0;
            }
         }
      }

      /* check to see if the pty is ready to read */
      if (FD_ISSET(master, &inmask)) {
         pty_bytes = read(master, pty_buffer, sizeof(pty_buffer));
         if (pty_bytes < 0) {
            if (errno == EWOULDBLOCK) {
               pty_bytes = 0;
               errno = 0;
            }
				else finished = 1;
         }
			else if (pty_bytes == 0)
				finished = 1;
      }

      /* check to see if the is ready for a write */
      if ((pty_bytes > 0) && (FD_ISSET(s, &outmask))) {
         p_pty_buffer = pty_buffer;
         while (pty_bytes > 0) {
            num_bytes = write(s, p_pty_buffer, pty_bytes);
            if (num_bytes > 0) {
               pty_bytes -= num_bytes;
               p_pty_buffer += num_bytes;
            }
            else if (num_bytes <= 0) {
               if (errno != EWOULDBLOCK) {
						sigaction(SIGCHLD, &old_action, (struct sigaction *)NULL);
                  return 1;
					}
               else
                  errno = 0;
            }
         }
      }
   }
}

/*
 ******************************************************************************
 *
 * Name:        pty_client_io
 *
 * Description: This function is called by a client process.  It reads data
 *              from standard input and writes it to a socket.  The data it
 *              reads from the socket is written to standard output.  (50128)
 *
 * Parameters:  s - The file descriptor for the socket.
 *
 * Returns:     0 if successful, -1 on error
 * 
 * History:     08/03/05 mjb 15056 - Moved the logic for setting the s_timeout
 *                                   structure into the infinite loop.  This
 *                                   ensures the timeout value is properly
 *                                   set when the "select" call is made.  This
 *                                   issue appeared with the migration to
 *                                   Linux.
 *
 ******************************************************************************
*/
int pty_client_io(int s, int ms, int to_min, int hb)
{
   int maxfd, num_bytes;
   register int std_bytes, fd_bytes;
   char std_buffer[1024], fd_buffer[1024];
   char *p_std_buffer, *p_fd_buffer;
   fd_set inmask, outmask, errmask, *p_outmask;
   struct timeval *p_timeout = (struct timeval *)NULL;
   struct timeval s_timeout = {0, 0};
   int fds_ready;
   int finished = 0;

   std_bytes = fd_bytes = 0;
   maxfd = s + 1;

   for ( ; ; ) {
      if (finished)
         return 0;

      if (to_min > 0) {
         s_timeout.tv_sec = 60 * to_min;
         p_timeout = &s_timeout;
      }

      FD_ZERO(&inmask);
      FD_ZERO(&outmask);
      FD_ZERO(&errmask);

      p_outmask = (fd_set *)NULL;

      if (fd_bytes > 0) {
			FD_SET(STDOUT_FILENO, &outmask);
         p_outmask = &outmask;
      }
      else
         FD_SET(s, &inmask);

      if (std_bytes > 0) {
         FD_SET(s, &outmask);
         p_outmask = &outmask;
      }
      else
         FD_SET(STDIN_FILENO, &inmask);

      FD_SET(ms, &inmask);
      FD_SET(s, &errmask);

      fds_ready = select(maxfd, &inmask, p_outmask, &errmask, p_timeout);
      if (fds_ready < 0) {
         if (errno == EINTR)
            continue;
         else
         return -1;
      }
      else if (fds_ready == 0) {
         if (hb != 0) {
            gs_send_heartbeat(ms);
         }

         continue;
      }

      if (FD_ISSET(s, &inmask)) {
         fd_bytes = read(s, fd_buffer, sizeof(fd_buffer));
         if (fd_bytes < 0) {
            if (errno == EWOULDBLOCK) {
               fd_bytes = 0;
               errno = 0;
            }
            else
               return -1;
         }
         else if (fd_bytes == 0)
            return 0;
      }

      if (FD_ISSET(STDOUT_FILENO, &outmask)) {
         p_fd_buffer = fd_buffer;
         while (fd_bytes > 0) {
            num_bytes = write(STDOUT_FILENO, p_fd_buffer, fd_bytes);
            if (num_bytes > 0) {
               fd_bytes -= num_bytes;
               p_fd_buffer += num_bytes;
            }
            else if (num_bytes < 0) {
               if (errno != EWOULDBLOCK)
                  return 1;
               else
                  errno = 0;
            }
         }
      }

      if (FD_ISSET(STDIN_FILENO, &inmask)) {
         std_bytes = read(STDIN_FILENO, std_buffer, sizeof(std_buffer));
         if (std_bytes < 0) {
            if (errno == EWOULDBLOCK) {
               std_bytes = 0;
               errno = 0;
            }
            else
               return -1;
         }
         else if (std_bytes == 0)
            return 0;
      }

      if (FD_ISSET(s, &outmask)) {
         p_std_buffer = std_buffer;
         while (std_bytes > 0) {
            num_bytes = write(s, p_std_buffer, std_bytes);
            if (num_bytes > 0) {
               std_bytes -= num_bytes;
               p_std_buffer += num_bytes;
            }
            else if (num_bytes < 0) {
               if (errno != EWOULDBLOCK)
                  return 1;
               else
                  errno = 0;
            }
         }
      }

		if (FD_ISSET(ms, &inmask)) {
			gs_inspect_message(ms);
		}
   }
}

/*
 ******************************************************************************
 *
 * Name:        pty_set_raw
 *
 * Description: This function turns off local echoing and the canonical mode
 *              to put the terminal in "raw" mode.  (50128)
 *
 * Parameters:  fd - File descriptor of terminal to set to raw mode.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int pty_set_raw(int fd)
{
   struct termios temp;

   if (tcgetattr(fd, &temp) < 0)
      return -1;

   temp.c_lflag &= ~(ECHO|ICANON);
   temp.c_cc[VMIN] = 1;
   temp.c_cc[VTIME] = 0;

   return ((tcsetattr(fd, TCSAFLUSH, &temp) < 0) ? -1 : 0);
}

/*
 ******************************************************************************
 *
 * Name:        pty_save_state
 *
 * Description: This function saves the current state of the terminal
 *              specified by the fd parameter.  (50128)
 *
 * Parameters:  fd        - File descriptor of the terminal.
 *              save_term - Pointer to termios structure where the terminal
 *                          state will be saved.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int pty_save_state(int fd, struct termios *save_term)
{
   if (save_term == NULL)
      return -1;

   return tcgetattr(fd, save_term);
}

/*
 ******************************************************************************
 *
 * Name: pty_restore_state
 *
 * Description: This function restores the terminal specified by the fd
 *              parameter to the state specified in the restore_term
 *              parameter.  (50128)
 *
 * Parameters:  fd           - File descriptor of terminal.
 *              restore_term - Pointer to the termios structure that is being
 *                             used to restore the terminal.
 *
 ******************************************************************************
*/
int pty_restore_state(int fd, struct termios *restore_term)
{
   if (restore_term == NULL)
      return -1;

   return tcsetattr(fd, TCSANOW, restore_term);
}

/*
 ******************************************************************************
 *
 * Name:        pty_get_window_size
 *
 * Description: This function gets the window size of the terminal
 *              specified by the fd parameter.  (50128)
 *
 * Parameters:  fd       - File descriptor of the terminal.
 *              win_size - Pointer to winsize structure where the window
 *                         size will be saved.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int pty_get_window_size(int fd, struct winsize *win_size)
{
   if (win_size == NULL)
      return -1;

   return ioctl(fd, TIOCGWINSZ, win_size);
}

/*
 ******************************************************************************
 *
 * Name:        pty_set_window_size
 *
 * Description: This function sets the window size of the terminal
 *              specified by the fd parameter.  (50128)
 *
 * Parameters:  fd       - File descriptor of the terminal.
 *              win_size - Pointer to winsize structure that contains the
 *                         new window size.
 *
 * Returns:     0 if successful, -1 on error
 *
 ******************************************************************************
*/
int pty_set_window_size(int fd, struct winsize *win_size)
{
   if (win_size == NULL)
      return -1;

   return ioctl(fd, TIOCSWINSZ, win_size);
}
