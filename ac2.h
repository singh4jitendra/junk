#if !defined(_AC2_H)
#define _AC2_H

#if !defined(_STDIO_H)
#include <stdio.h>
#endif

#define AC2_OS_UNIX  'U'
#define AC2_OS_WINNT 'N'
#define AC2_OS_WIN95 'W'

#define AC2_AUTHORIZATION_FAILED      -1
#define AC2_CONNECTION_NOT_AUTHORIZED  0
#define AC2_CONNECTION_AUTHORIZED      1
#define AC2_HOST_NOT_FOUND             2

#define AC2_FLAG_UNIX_OPTIONAL_LOGIN   0x0001
#define AC2_FLAG_WIN95_ALWAYS_LOGIN    0x0002
#define AC2_FLAG_WINNT_NEVER_LOGIN     0x0004
#define AC2_FLAG_WINNT_ALWAYS_LOGIN    0x0008
#define AC2_FLAG_WIN_ALWAYS_LOGIN      0x000A

struct ac2 {
	char ip_address[20];
	char host_name[50];
	int authorized_to_connect;
	int requires_login;
	int is_shinv_branch;
	char os;
	int snapshot;
};

int AuthorizeConnection2(char *, char *, struct ac2 *);
int check_auth_file(FILE *, char *, struct ac2 *);

#endif
