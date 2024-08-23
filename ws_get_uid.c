/*
 ******************************************************************************
 * ws_get_uid.c - Obtains the user name associated with a user id.
 *
 * Development History:
 * 08/28/06  mjb  06154 - Initial development.
 ******************************************************************************
*/

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>

char * ws_get_user_by_uid(uid_t uid, char * username, size_t len)
{
    char * rv = NULL;
    struct passwd * pwd;
    
    if (username != NULL) {
        if ((pwd = getpwuid(uid)) != NULL) {
            snprintf(username, len, "%s", pwd->pw_name);
        }
        else {
            snprintf(username, len, "%d", (int)uid);
        }
        
        rv = username;
    }
    
    return rv;
}
