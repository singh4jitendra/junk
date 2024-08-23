#if !defined(_PIDFILE_H)
#define _PIDFILE_H

char *CreatePIDFile(char *, char *, char *);
pid_t ReadPIDFile(char *);

#endif
