#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ac2.h"

int AuthorizeConnection2(char * lpDbName, char * lpHost, struct ac2 * aci)
{
	FILE *fp;
	char * p;
	int done = 0;
	char hn[256];
	int flags = 0;
	int return_code = AC2_CONNECTION_NOT_AUTHORIZED;

	memset(aci, 0, sizeof(struct ac2));

	if ((fp = fopen(lpDbName, "r")) == NULL) {
		return AC2_AUTHORIZATION_FAILED;
	}

	strcpy(hn, lpHost);

	do {
		return_code = check_auth_file(fp, hn, aci);
		rewind(fp);  /* set the file pointer to the start of the file */
		if ((p = strrchr(hn, '.')) == NULL) {
			done = 1;
		}
		else {
			*p = 0;
		}
	} while ((return_code == AC2_HOST_NOT_FOUND) && !done);

	if (return_code == AC2_HOST_NOT_FOUND) {
		strcpy(aci->ip_address, lpHost);
		strcpy(aci->host_name, lpHost);

		return_code = AC2_CONNECTION_NOT_AUTHORIZED;
	}

	fclose(fp);
	return return_code;
}

int check_auth_file(FILE *fp, char * host_name, struct ac2 * aci)
{
	int i = 0;
	long flag;
	struct ac2 info;
	char *begin, *end;
	char szBuffer[256];
	int return_code = AC2_HOST_NOT_FOUND;

	while (1) {
		if (fgets(szBuffer, sizeof(szBuffer), fp) == NULL) {
			if (feof(fp)) {
				return AC2_HOST_NOT_FOUND;
			}
			else {
				return AC2_AUTHORIZATION_FAILED;
			}
		}

		if (*szBuffer == '#') /* this line is a comment */
			continue;

		if ((end = strrchr(szBuffer, '\n')) != NULL) *end = 0;

		memset(&info, 0, sizeof(info));

		/* Parse ip_address from the record */
		begin = szBuffer;
		if ((end = strchr(begin, ':')) == NULL) continue;
		*end = 0;
		strncpy(info.ip_address, begin, sizeof(info.ip_address));

		/* Parse host_name from the record */
		begin = end + 1;
		if ((end = strchr(begin, ':')) == NULL) continue;
		*end = 0;
		strncpy(info.host_name, begin, sizeof(info.host_name));

		/* Parse connection_authorized from the record */
		begin = end + 1;
		if ((end = strchr(begin, ':')) == NULL) continue;
		*end = 0; if (strlen(begin) != 1) continue;
		info.authorized_to_connect = *begin - '0';
		if ((strlen(begin) != 1) || ((*begin != '0') && (*begin != '1')))
			info.authorized_to_connect = 1;
		else
			info.authorized_to_connect = *begin - '0';

		/* Parse requires_login from the record */
		begin = end + 1;
		if ((end = strchr(begin, ':')) == NULL) continue;
		*end = 0; if (strlen(begin) != 1) continue;
		if ((strlen(begin) != 1) || ((*begin != '0') && (*begin != '1')))
			info.requires_login = 1;
		else
			info.requires_login = *begin - '0';

		/* Parse is_shinv_branch from the record */
		begin = end + 1;
		if ((end = strchr(begin, ':')) == NULL) continue;
		*end = 0; if (strlen(begin) != 1) continue;
		if ((strlen(begin) != 1) || ((*begin != '0') && (*begin != '1')))
			info.is_shinv_branch = 0;
		else
			info.is_shinv_branch = *begin - '0';

		if (info.is_shinv_branch != 0) info.is_shinv_branch = 1;

		/* Parse os from the record */
		begin = end + 1;
		end = strchr(begin, ':');
		info.os = toupper(*begin);
		if ((info.os != 'U') && (info.os != 'N') &&(info.os != 'W'))
			continue;

		if (end == NULL) {
			info.snapshot = 0;
		}
		else {
			begin = end + 1;
			if ((strlen(begin) != 1) || ((*begin != '0') && (*begin != '1')))
				info.snapshot = 0;
			else
				info.snapshot = *begin - '0';
		}

		if (strcmp(host_name, info.ip_address) == 0) {
			/* entry exists in authorization file */
			memcpy(aci, &info, sizeof(struct ac2));

			if (aci->authorized_to_connect) {
				switch (toupper(aci->os)) {
					case AC2_OS_UNIX:
						return AC2_CONNECTION_AUTHORIZED;
					case AC2_OS_WINNT:
						return AC2_CONNECTION_AUTHORIZED;
					case AC2_OS_WIN95:
						if (!aci->requires_login) aci->requires_login = 1;
						return AC2_CONNECTION_AUTHORIZED;
					default:
						// Invalid OS specified, deny access
						return AC2_CONNECTION_NOT_AUTHORIZED;
				}

				break;
			}
			else {
				return_code = AC2_CONNECTION_NOT_AUTHORIZED;
			}
		}
	}

	return AC2_CONNECTION_NOT_AUTHORIZED;
}
