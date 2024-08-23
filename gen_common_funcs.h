#ifndef _GEN_COMMON_FUNCS_H
#define _GEN_COMMON_FUNCS_H

#if defined(_WIN32)
#   define CHDIR_FAILED       0
#else
#   define CHDIR_FAILED      -1
#endif

char * gs_set_cfg_file(char * cfg);
int ws_recv_packet(wesco_socket_t s, wesco_void_ptr_t buffer, size_t buffsize);
int ws_send_packet(wesco_socket_t s, wesco_void_ptr_t buffer, size_t buffsize);
int gs_send_pg_packet(wesco_socket_t s, short opcode, char * filename,
	mode_t perms, char * user, char * group);
int gs_put_file(wesco_socket_t s, char * filename, mode_t perms, char * user,
	char * group);
int gs_get_file(wesco_socket_t s, char * filename, mode_t perms, char * user,
	char * group);
int gs_send_cd_packet(wesco_socket_t s, char * directory);
int ws_chdir(char * directory);
int gs_send_cdup(wesco_socket_t s);
int gs_send_run(wesco_socket_t s, short redirect, LPGENTRANSAREA parms);
int gs_get_gentransarea(wesco_socket_t s, LPGENTRANSAREA parms);
int gs_get_multiple_files(wesco_socket_t s);
int gs_put_multiple_files(wesco_socket_t s, char * pattern);
int gs_send_ack_packet(wesco_socket_t s, short opcode);
int RemoteFiletoPG(LPPGPACKET pg, LPREMOTEFILE rf);
int setgrown(char * filename, char * owner, char * group);
int getinfo(int fd, mode_t *mode, char * owner, char * group);
int gs_send_pp_packet(wesco_socket_t s, short opcode, char * func);
int gs_send_ext_packet(wesco_socket_t s);
int gs_send_error_packet(wesco_socket_t s, char * lpStatus, char * lpMsg);
int do_genserver_commands(wesco_socket_t s, int32_t *gen_flags, LPGENTRANSAREA parms, int heartbeat);
wesco_socket_t ConnectToRemote(LPGENTRANSAREA parms);
bool_t NegotiateVersion(wesco_socket_t s, LPGENVERSION lpGenVersion, LPGENTRANSAREA parms);
bool_t ReadRegReleaseInfo(LPGENVERSION lpGenVersion , const char * lpVersionKey);
int gs_get_file_info(int fd, mode_t *pMode, char *lpOwner, size_t nOwnerSize,
	char *lpGroup, size_t nGroupSize);

#endif
