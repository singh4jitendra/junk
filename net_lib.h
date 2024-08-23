/*
   Changed: 12/22/04 mjb - Project 04041. Added a preconditional statement
                           to define PIPE_MAX if it is not already defined.
*/
#if !defined(_NET_LIB_H)
#define _NET_LIB_H

#if !defined(_STDARG_H)
#   include <stdarg.h>
#endif

#define EXIT_NO_MEM 5
#define EXIT_NO_FILES 8

#ifndef PIPE_MAX
#define PIPE_MAX 5120
#endif

typedef struct FNAMES {
   struct FNAMES *next;
   char *name;
} FNAMES;

int transmit_all_files(int, char *, FNAMES *, THROUGHPUT_INFO *);
int receive_all_files(int, char *, THROUGHPUT_INFO *);
int send_init_comm(int, int *);
int send_tx_start(int);
int send_tx_end(int);
int send_rx_start(int);
int send_rx_end(int);
int send_file_start(int, char *, char *, int32_t *);
int send_file_data(int, char *, int32_t);
int send_file_end(int);
int send_cmd_ack(int, int, int, char *, ...);
int send_end_comm(int);
int transmit_file(int, char *, char *, THROUGHPUT_INFO *);
int receive_file(int, int, char *, char *, int32_t *, THROUGHPUT_INFO *);
void net_throughput(THROUGHPUT_INFO *);
int interesting_name(char *);
void insert_file_alpha(FNAMES **, FNAMES *);
FNAMES *build_file_list(char *);
void free_list(struct FNAMES *);
void print_msg(char *, ...);
int recv_packet(int, void *, size_t);
size_t send_packet(int, void *, size_t);
int recv_data(int, void *, size_t, int);
int send_data(int, void *, size_t, int);
void set_timeout(int);

#endif
