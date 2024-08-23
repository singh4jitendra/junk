/*
 ******************************************************************************
 * get_process_owner.c - Returns the effective user id of the process specified
 *                       by the supplied process id.
 *
 * Development History:
 * 08/28/06  mjb  06154 - Initial development.
 ******************************************************************************
*/

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>

const char * uid_label = "Uid:";

uid_t ws_get_process_owner(pid_t pid)
{
    FILE * fp;
    char buffer[256];
    char filename[PATH_MAX];
    uid_t uid = (uid_t)0xffffffff;
    
    snprintf(filename, sizeof(filename), "/proc/%d/status", (int)pid);
    
    if ((fp = fopen(filename, "r")) != NULL) {
        while (!feof(fp)) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                if (strncmp(buffer, uid_label, strlen(uid_label)) == 0) {
                    uid = (uid_t)strtoul(buffer + strlen(uid_label), NULL, 0);
                    break;
                }
            }
        }
        
        fclose(fp);
    }
    else {
        uid = (uid_t)0xffffffff;
    }
    
    return uid;
}
