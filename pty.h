#ifndef _PTY_H_
#define _PTY_H_

int ptym_open(char *);
int ptys_open(char *);
pid_t pty_fork(int *, char *, struct termios *, struct winsize *);

/** 50128 */
int pty_server_io(int, int, int, int, int, int);
int pty_client_io(int, int, int, int);
int pty_set_raw(int);
int pty_save_state(int, struct termios *);
int pty_restore_state(int, struct termios *);
int pty_get_window_size(int, struct winsize *);
int pty_set_window_size(int, struct winsize *);

#endif
