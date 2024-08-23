#if !defined(_SIGPOLL_SETUP_H)
#define _SIGPOLL_SETUP_H

int set_socket_owner(int s, pid_t * owner);
int set_file_async(int fd, int state);
int set_file_nonblocking(int fd, int state);
void request_sigpoll(int s, int events);

#endif
