#if !defined(_GEN_CLIENT_FUNCS_H)
#define _GEN_CLIENT_FUNCS_H

int client_put_file(wesco_socket_t s, LPGENTRANSAREA lpGtaParms);
int client_get_file(wesco_socket_t s, LPGENTRANSAREA lpGtaParms);
int client_send_run(wesco_socket_t s, LPGENTRANSAREA lpGtaParms, int heartbeat);
int client_put_multiple_files(wesco_socket_t s, LPGENTRANSAREA lpGtaParms);
int client_get_multiple_files(wesco_socket_t s, LPGENTRANSAREA lpGtaParms);
int client_get_ack(wesco_socket_t s, LPGENTRANSAREA lpGtaParms);

#endif
