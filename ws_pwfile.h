#ifndef _WS_PWFILE_H_
#define _WS_PWFILE_H_

#ifndef _WS_COMPAT_H
#include "msdefs.h"
#endif

#ifndef _GRP_H
#include <grp.h>
#endif

typedef long pfdate_t;

struct pf_entry {
	char pf_name[10];
	char pf_passwd[50];
	char pf_start[50];
	char pf_langcode;
	char pf_lastaccess[6];
	char pf_lastchange[6];
	long pf_attempts;
	char pf_sflag;
};

void ws_pfclose();
int ws_getlastpferror();
int ws_pfopen();
struct pf_entry * ws_get_pfentry_by_name(const char * name);
bool_t ws_authenticate_login(const char * name, const char * pwd);
bool_t ws_set_password(const char * name, const char * pwd);
bool_t ws_change_password(const char * name, const char * pwd);
bool_t ws_chg_pwd(const char * name, const char * oldpwd, const char * newpwd);
pfdate_t ws_get_current_pf_date(void);
void ws_format_pf_today(char * buffer);
pfdate_t ws_calculate_pf_date(const char * pfdate);
struct pf_entry * ws_getpfent(void);
void ws_setpfent(void);
bool_t ws_parse_pf_ent(char * buffer, struct pf_entry * pfe);
int ws_pflock(void);
bool_t ws_update_pf_file(struct pf_entry * pfe, int mode);
int ws_pfunlock(void);
gid_t ws_groupid(void);

#ifdef PF_SALT_MAX
#undef PF_SALT_MAX
#endif
#define PF_SALT_MAX 6

extern int pf_errno;

#ifdef PF_ATTEMPTS_MAX
#undef PF_ATTEMPTS_MAX
#endif
#define PF_ATTEMPTS_MAX 7

#define PF_SUCCESS            0000
#define PF_INVALID_PASSWORD   1000
#define PF_INVALID_USER       1001
#define PF_PWDFILE_LOCKED     1002
#define PF_PASSWORD_EXPIRED   1003
#define PF_USER_LOCKED        1004
#define PF_PASSWORD_LENGTH    1005
#define PF_EXISTING_PASSWORD  1006
#define PF_INVALID_FORMAT     1007
#define PF_PERMISSION_DENIED  1008
#define PF_ILLEGAL_PASSWORD   1009
#define PF_UNCOMPLEX_PASSWORD 1010

#define PF_FILENAME "/usr/security/pwdfile"
#define PF_LOCKFILE "/tmp/pwdfile.lock"
#define PF_NEWFILE  "/usr/security/npwdfile"

#define PF_DELUSER 0x0001
#define PF_UPDUSER 0x0002
#define PF_ADDUSER 0x0004

#endif //_WS_PWFILE_H_
