#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 ******************************************************************************
 *
 * Name:        GetPrivateProfileString
 *
 * Description: This function retrieves a string from an initialization file.
 *
 * Parameters:  section  - INI file section that entry is in.
 *              key      - Entry name.
 *              buf      - Pointer to buffer to put string into.
 *              buffsize - Size of the buffer.
 *              defalt   - Default value.
 *              filename - Name of the INI file.
 *
 * Returns:     0 if successful; -1 on error.
 *
 * Modified:    05/24/05 04041 - Added logic to ensure strchr returned a
 *                               non-NULL value.
 ******************************************************************************
*/
int GetPrivateProfileString(char *section, char *key, char *buf, size_t buffsize, char *defalt, char *filename)
{
    FILE *fd;
    char buffer[1024];
    int insection = 0;
    int done = 0;
    char * pos = NULL;

    if ((fd = fopen(filename, "rt")) == NULL) {
        if (defalt != NULL)
            strcpy(buf, defalt);
        else {
            memset(buf, 0, buffsize);
            return -1;
        }

        return strlen(buf);
    }

    /* This loop will be executed until either the end of file is reached
       or the desired key is found. */
    while (!done) {
        fgets(buffer, sizeof(buffer), fd);

        if (feof(fd)) {
            /* The end of file was reached without finding the desired key.
               The defalt string will be copied in if it is not NULL. */
            if (defalt)
                strcpy(buf, defalt);
            done = 1;
        }

        switch (*buffer) {
            case ';' :
                /* if line starts with ; it is a remark */
                continue;
            case '[' :
                /* if line starts with [ it is a section header */
                if (!strncmp(section, buffer + 1, strlen(section)))
                    insection = 1;
                else
                    insection = 0;
                break;
            default:
                if (insection && !strncmp(key, buffer, strlen(key))) {
                    if ((pos = strchr(buffer, '\n')) != NULL) {
                        *pos = 0; /* remove cr/lf */
                    }

                    if ((pos = strchr(buffer, '=')) != NULL) {
                        strcpy(buf, pos + 1);
                    }
                    else {
                        if (defalt) {
                            strcpy(buf, defalt);
                        }
                    }
                    done = 1;
                }
        }
    }
 
    fclose(fd);

    return strlen(buf);
}

/*
 ******************************************************************************
 *
 * Name:        GetPrivateProfileInt
 *
 * Description: This function retrieves a string from an initialization file.
 *
 * Parameters:  section  - INI file section that entry is in.
 *              key      - Entry name.
 *              defalt   - Default value.
 *              filename - Name of the INI file.
 *
 * Returns:     0 if successful; -1 on error.
 *
 ******************************************************************************
*/
int GetPrivateProfileInt(char *section, char *key, int defalt, char *filename)
{
    char buffer[25];
    char cdefalt[25];

    sprintf(cdefalt, "%d", defalt);
    GetPrivateProfileString(section, key, buffer, sizeof(buffer), cdefalt, filename);

    return atoi(buffer);
}
