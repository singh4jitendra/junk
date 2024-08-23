#ifndef _GEN_SERVER_FUNCS_H
#define _GEN_SERVER_FUNCS_H

#if defined(_HPUX_SOURCE)
#define WS_PASSWD_FILE  "/dps/genserver/pwdfile"
#else
#define WS_PASSWD_FILE  "/usr/security/pwdfile"
#endif

#define WS_PASSWD_ERR   1
#define WS_USERNAME_ERR 2

#define GS_IP_DBNAME    "/IMOSDATA/2/ipdb"
#define GS_W2HOSTS      "/IMOSDATA/2/w2hosts"

#if !defined(_EMBEDDED_COBOL)
typedef void Argument;
#endif

int genserver_get_file(wesco_socket_t s, LPPGPACKET lpPgPacket);
int genserver_put_file(wesco_socket_t s, LPPGPACKET lpPgPacket);
int genserver_run(wesco_socket_t s, LPRUNPACKET lpRunPacket, char * lpUserDir, int heartbeat);
int genserver_chdir(wesco_socket_t s, LPCDPACKET lpCdPacket);
int genserver_get_multiple_files(wesco_socket_t s, LPPGPACKET lpPgPacket);
int genserver_put_multiple_files(wesco_socket_t s, LPPGPACKET lpPgPacket);
int genserver_create_user_directory(wesco_socket_t s, LPPARTPACKET lpPartPacket,
												char * lpDirName);
int genserver_delete_user_directory(wesco_socket_t s, char * lpDirName);
int genserver_run_with_io_redirection(wesco_socket_t s, LPGENTRANSAREA lpGtaParameters,
												  int *pCobolArgc, char * lpCobolArgv[],
									           Argument *pCobolArgs,
												  int heartbeat);
int genserver_run_no_io_redirection(wesco_socket_t s, LPGENTRANSAREA lpGtaParameters,
												int *pCobolArgc, char * lpCobolArgv[],
												Argument *pCobolArgs);
int gs_check_login(wesco_socket_t s, LPPWDPACKET lpPwdPacket);
bool_t gs_in_ip_family(char * lpDbName, char * lpHost);
struct ws_pfent *ws_getpfname(char *);

#endif
