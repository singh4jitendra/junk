#ifndef LINE_H
#define LINE_H

int send_line(struct DIAL_PARMS *, char *, int);
int get_line(struct DIAL_PARMS *, char *, int, int);
int get_line_buf(struct DIAL_PARMS *,char *, int, int);
int open_line(struct DIAL_PARMS *);
void flush_line(struct DIAL_PARMS *);
void dtr_down(struct DIAL_PARMS *);
void dtr_up(struct DIAL_PARMS *);
void flow_off(struct DIAL_PARMS *);
int set_blocking(struct DIAL_PARMS *);
int set_nonblocking(struct DIAL_PARMS *);
int get_dcd(struct DIAL_PARMS *);
int get_cts(struct DIAL_PARMS *);

#define GETLINE_OK 0
#define GETLINE_TIMEOUT -2
#define BAD_SCRIPT -3

#define LINE_TRIES 2

#ifdef DOS
void sleep(int);
#endif

#endif
