#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <grp.h>
#include <pwd.h>

#define GENLIB

#include "common.h"
#include "generic.h"
#include "gen2.h"


/*
 ******************************************************************************
 *
 * Name:        recvpacket
 *
 * Description: This function receives data from a connected socket.  It 
 *              receives the size of transaction and then receives the
 *              transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to read data into.
 *              buffsize - Size of the buffer.
 *
 * Returns:     Number of bytes read in; or, -1 on error.
 *
 ******************************************************************************
*/
int recvpacket(int s, void *buffer, short buffsize)
{
    int total = 0;
	 short packet_size;
    
    if (recvdata(s, (char *)&packet_size, sizeof(short), 0) < 0) {
      return -1;
    }

    packet_size = endianswap(packet_size);

	 if (packet_size < buffsize)
		  buffsize = packet_size;

    if ((total = recvdata(s, buffer, buffsize, 0 )) < 0) {
      return -1;
    }

    return total;
}

/*
 ******************************************************************************
 *
 * Name:        sendpacket
 *
 * Description: This function sends data on a connected socket.  It
 *              sends the size of transaction and then sends the
 *              transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to write data from.
 *              buffsize - Size of the buffer.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int sendpacket(int s, void *buffer, short buffsize)
{
    short temp = endianswap(buffsize);

    if (senddata(s, (char *)&temp, sizeof(short), 0) < sizeof(short))
        return -1;

    if (senddata(s, (char *)buffer, buffsize, 0) < buffsize)
        return -1;

    return buffsize;
}

/*
 ******************************************************************************
 *
 * Name:        sendpg
 *
 * Description: This function "put/get" transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              opcode   - Specifies PUT or GET.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int sendpg(int s, short opcode, char *filename, mode_t perms, char *owner, 
    char *group)
{
    int bytes;
    int length;
    PGPACKET buffer;

    /* zero out the buffer */
    memset(&buffer, 0, sizeof(PGPACKET));

    buffer.opcode = endianswap(opcode);

    if ((length = strlen(filename)) > FILENAMELENGTH) {
        generr = EFILENAMELENGTH;
        return -1;
    }

    strcpy(buffer.filename, filename);

    buffer.permissions = (int32_t)perms;

    if (owner != NULL)
      strcpy(buffer.owner, owner);
    
    if (group != NULL)
      strcpy(buffer.group, group);

    length = sizeof(PGPACKET);
    if ((bytes = sendpacket(s, &buffer, length)) < length)
        return -1;

    return bytes;
}

/*
 ******************************************************************************
 *
 * Name:        putfile
 *
 * Description: This function sends a file to the remote system.
 *
 * Parameters:  s        - Socket descriptor.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int putfile(int s, char *filename, mode_t perms, char *owner, char *group)
{
    int bytes;
    int length;
    int fd;
    int done = FALSE;
    DATAPACKET dp;
    mode_t st_perms;
    mode_t *ptr_perms;
    char st_owner[OWNERLENGTH+1];
    char st_group[GROUPLENGTH+1];
    char *ptr_owner;
    char *ptr_group;
    char *filebase;
    char *temp;
    char truename[FILENAMELENGTH];
    char tmp[FILENAMELENGTH];
    int32_t t1, t2;

    if ((fd = open(filename, O_RDONLY)) == 0) {
        generr = EFILEEXIST;
        return -1;
    }

    filebase = strdup(filename);

    if (perms > 0)
        ptr_perms = (mode_t *)NULL;
    else
        ptr_perms = &st_perms;

    ptr_owner = (owner == NULL) ? st_owner : (char *)NULL;
    ptr_group = (group == NULL) ? st_group : (char *)NULL;

    if (getinfo(fd, ptr_perms, ptr_owner, ptr_group) < 0) {
        generr = EFILEEXIST;
        return -1;
    }

    if (perms == 0)
      perms = *ptr_perms;

    if (owner == NULL)
      owner = ptr_owner;

    if (group == NULL)
      group = ptr_group;

    temp = basename(filebase);
    strcpy(truename, temp);

    if (!strncmp(truename, "genqueue", 8)) {
       sscanf(truename+9, "%lu.%lu.%s", &t1, &t2, tmp);
    }
    else
       strcpy(tmp, truename);

    if (sendpg(s, PUT, tmp, perms, owner, group) < 0)
        return -1;

    dp.opcode = endianswap(DATA);

    while (!done) {
        length = read(fd, dp.buffer, sizeof(dp.buffer));

        if (length < 1)
            length = 0;

        if (length < sizeof(dp.buffer))
            dp.opcode = endianswap(EOFTX);

        bytes = sendpacket(s, &dp, length + sizeof(dp.opcode));
        if (bytes < (length + sizeof(dp.opcode))) {
            return -1;
        }

        if (length < sizeof(dp.buffer))
            done = TRUE;
    }

    close(fd);

    return 0;
}

void ws_hexdump(FILE *fp, char *buffer, size_t len)
{
	int i;
	union {
		char b[2];
		short s;
	} ud;

	ud.s = 0;
	for (i = 0; i < len; i++) {
		if (!(i%25))
			fprintf(fp, "\n");

		ud.b[0] = buffer[i];
		fprintf(fp, "%02hx ", ud.s);
	}

	fprintf(fp, "\n");
}

/*
 ******************************************************************************
 *
 * Name:        getfile
 *
 * Description: This function receives a file from the remote system.
 *
 * Parameters:  s        - Socket descriptor.
 *              filename - Name of file to transfer.
 *              perms    - Permissions for the file.
 *              owner    - Owner of the file.
 *              group    - Owner's group.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int getfile(int s, char *filename, mode_t perms, char *owner, char *group)
{
    int bytes;
    int fd;
    int done = FALSE;
    DATAPACKET *dp;
    PGPACKET *pg;
    ERRORPACKET *err;
    short *opcode;
    char buffer[1024];
    mode_t oldmask = umask(0);
#if defined(_HPUX_SOURCE)
    mode_t *hp_mode;
#endif

#if defined(_DEBUG_DUMP_PACKETS)
FILE *fp;
void fp_hexdump(FILE *, char *, size_t);
#endif

    if (recvpacket(s, buffer, sizeof(buffer)) < 0) {
        umask(oldmask);
        return -1;
    }

    opcode = (short *)buffer;

    switch(endianswap(*opcode)) {
        case ERROR:
            err = (ERRORPACKET *)buffer;
            umask(oldmask);
            return -1;
        default:
            pg = (PGPACKET *)buffer;
            break;
    }

    if (!filename)
        filename = pg->filename;

#if defined(_DEBUG_DUMP_PACKETS)
fp = fopen("/tmp/getfile.dat", "a+");
fprintf(fp, "PGPACKET:\n");
ws_hexdump(fp, (char *)pg, sizeof(PGPACKET));
#endif

#if defined(_HPUX_SOURCE)
    if (!perms) {
        hp_mode = (mode_t *)(&(pg->permissions));
        perms = endianswap(*hp_mode);
    }
#else
    if (!perms)
        perms = pg->permissions;
#endif

    if (!owner || owner[0] <= 32)
        owner = pg->owner;

    if (!group || group[0] <= 32)
        group = pg->group;

    if (filename[0] == 0) {
        umask(oldmask);
        return -2;
    }

    if ((fd = creat(filename, perms)) < 0) {
        umask(oldmask);
        return -1;
    }

    if (endianswap(*opcode) != DONE) {
       while (!done) {
           if ((bytes = recvpacket(s, buffer, sizeof(buffer))) < 0) {
               umask(oldmask);
               return -1;
           }

#if defined(_DEBUG_DUMP_PACKETS)
fprintf(fp, "buffer:\n");
ws_hexdump(fp, (char *)buffer, sizeof(buffer));
#endif

           opcode = (short *)buffer;
           switch (endianswap(*opcode)) {
               case ERROR:
                   done = TRUE;
                   err = (ERRORPACKET *)buffer;
                   break;
               case EOFTX:
                   done = TRUE;
               case DATA:
                   dp = (DATAPACKET *)buffer;
                   write(fd, dp->buffer, bytes - sizeof(dp->opcode));
           }
       }
    }
    else {
       perms = S_IRWXU|S_IRWXG|S_IRWXO;
       owner = NULL;
       group = NULL;
    }

    close(fd);

#if defined(_DEBUG_DUMP_PACKETS)
fclose(fp);
#endif

    if (perms)
        chmod(filename, perms);
    else
        chmod(filename, S_IRWXU|S_IRWXG|S_IRWXO);
    
    setgrown(filename, owner, group);

    umask(oldmask);

    return 0;
}

/*
 ******************************************************************************
 *
 * Name:        sendcd
 *
 * Description: This function sends a "change directory" transaction.
 *
 * Parameters:  s         - Socket descriptor.
 *              directory - Name of to change to.
 *
 * Returns:     Number of bytes written in; or, -1 on error.
 *
 ******************************************************************************
*/
int sendcd(int s, char *directory)
{
    int bytes;
    int length;
    char buffer[280];
    CDPACKET *cd;

    cd = (CDPACKET *)buffer;
    memset(cd, 0, sizeof(cd));

    cd->opcode = endianswap(CD);

    if ((length = strlen(directory)) > FILENAMELENGTH) {
        generr = EFILENAMELENGTH;
        return -1;
    }

    strcpy(cd->dirname, directory);

    length = sizeof(CDPACKET);
    if ((bytes = sendpacket(s, cd, length)) < length)
        return -1;

    return bytes;
}

/*
 ******************************************************************************
 *
 * Name:        wescocd
 *
 * Description: This function changes the current directory.
 *
 * Parameters:  directory - Name of to change to.
 *
 * Returns:     0 if successful; -1 on error.
 *
 ******************************************************************************
*/
int wescocd(char *directory)
{
    if (chdir(directory)) {
        return -1;
    }

    return 0;
}

/*
 ******************************************************************************
 *
 * Name:        cdup
 *
 * Description: This function sends a "change directory" transaction
 *              to go up one level.
 *
 * Parameters:  s - Socket descriptor.
 *
 * Returns:     Result of sendcd().
 *
 ******************************************************************************
*/
int cdup(int s)
{
    return sendcd(s, "..");
}

/*
 ******************************************************************************
 *
 * Name:        sendrun
 *
 * Description: This function sends a "RUN" transaction.
 *
 * Parameters:  s        - Socket descriptor.
 *              redirect - Flag to redirect io.
 *              parms    - Cobol parameters.
 *
 * Returns:     Number of bytes sent; or, -1 on error.
 *
 ******************************************************************************
*/
int sendrun(int s, short redirect, GENTRANSAREA *parms)
{
    int length;
    int bytes;
    RUNPACKET buffer;

    memset(&buffer, 0, sizeof(buffer));

    buffer.opcode = endianswap(RUN);
    buffer.redirect = redirect;

    length = sizeof(buffer);
    if ((bytes = sendpacket(s, &buffer, length)) < length) {
        generr = ESEND;
        return -1;
    }

    length = sizeof(GENTRANSAREA);
    if ((bytes = sendpacket(s, parms, length)) < length) {
        generr = ESEND;
        return -1;
    }

    return (bytes + sizeof(buffer));
}

int getparms(int s, GENTRANSAREA *parms)
{
    int bytes;

    if ((bytes = recvpacket(s, parms, sizeof(GENTRANSAREA))) < 0)
        generr = ERECEIVE;
    
    return bytes;
}

int mget(int s)
{
    int done = FALSE;

    do {
        switch (getfile(s, NULL, 0, NULL, NULL)) {
            case -2:
                done = TRUE;
                break;
            case -1:
                generr = ERECEIVE;
                return -1;
            default:
                done = FALSE;
        }

    } while (!done);
    
    return 0;
}

int mput(int s, char *pattern)
{
   char fullname[FILENAMELENGTH];
   char filename[FILENAMELENGTH+1];
   char path[FILENAMELENGTH+1];
   struct stat fs;
   DIR *dirp;
   struct dirent *entry;

   ParseFilename(pattern, path, filename);

   if ((dirp = opendir(path)) == NULL)
      return -1;

   while ((entry = readdir(dirp)) != NULL) {
      if (file_pattern_match(entry->d_name, filename)) {
         sprintf(fullname, "%s/%s", path, entry->d_name);
         if (stat(fullname, &fs) != -1) {
            /* we only want to send regular files */
            if (S_ISREG(fs.st_mode)) {
               if (putfile(s, fullname, 0, NULL, NULL) < 0) {
                  generr = ESEND;
                  senddone(s);
                  return -1;
               }
            }
         }
      }
   }

    senddone(s);
    
    return 0;
}

int senddone(int s)
{
    char buffer[1024];
    PGPACKET *pg;

    pg = (PGPACKET *)buffer;
    memset(pg, 0, sizeof(PGPACKET));
    pg->opcode = endianswap(DONE);
    if (sendpacket(s, pg, sizeof(PGPACKET)) == -1) {
        generr = ESEND;
        return -1;
    }

    return 0;
}

int sendack(int s, short opcode)
{
    int bytes;
    ACKPACKET ack;

    ack.opcode = endianswap(ACK);
    ack.acking = endianswap(opcode);

    if ((bytes = sendpacket(s, &ack, sizeof(ack))) < 0)
        generr = ERECEIVE;

    return bytes;
}

int RemoteFiletoPG(PGPACKET *pg, REMOTEFILE *rf)
{
    char temp[PERMISSIONSLENGTH + 1];

    memset(pg, 0, sizeof(PGPACKET));

    CobolToC(pg->filename, rf->name, sizeof(pg->filename), sizeof(rf->name));
    CobolToC(temp, rf->permissions, sizeof(temp), sizeof(rf->permissions));
    CobolToC(pg->owner, rf->owner, sizeof(pg->owner), sizeof(rf->owner));
    CobolToC(pg->group, rf->group, sizeof(pg->group), sizeof(rf->group));

    sscanf(temp, "%o", &(pg->permissions));

    return 0;
}

int setgrown(char *filename, char *owner, char *group)
{
    struct group *groupEntry;
    struct passwd *passwordEntry;
    gid_t cur_gid;
    uid_t cur_uid;

    /* If the group does not exist, make it wescom */
    if (group == NULL)
        cur_gid = getgid();
    else if ((groupEntry = getgrnam(group)) == NULL) {
        groupEntry = getgrnam("wescom");
        cur_gid = groupEntry->gr_gid;
    }

    /* If the owner does not exist, make it super */
    if (owner == NULL)
        cur_uid = getuid();
    else if ((passwordEntry = getpwnam(owner)) == NULL) {
        passwordEntry = getpwnam("super");
        cur_uid = passwordEntry->pw_uid;
    }

    chown(filename, cur_uid, cur_gid);

    return 0;
}

int getinfo(int fd, mode_t *mode, char *owner, char *group)
{
    struct group *groupEntry;
    struct passwd *passwordEntry;
    struct stat fs;

    if (fstat(fd, &fs) == -1)
        return -1;

    if (mode != NULL)
        *mode = fs.st_mode;
    
    if (group != NULL) {
        groupEntry = getgrgid(fs.st_gid);
        strcpy(group, groupEntry->gr_name);
    }

    if (owner != NULL) {
        passwordEntry = getpwuid(fs.st_uid);
        strcpy(owner, passwordEntry->pw_name);
    }

    return 0;
}

int sendpp(int s, short opcode, char *func)
{
  int bytes;
  PARTPACKET pp;
  FILE *fp;

  memset(&pp, 0, sizeof(pp));

  pp.opcode = endianswap(opcode);

  if (func)
    strcpy(pp.func, func);

  if ((bytes = sendpacket(s, &pp, sizeof(pp))) < 0)
    return -1;

  return bytes;
}

int senderror(int s, char *gta_status, char *msg)
{
   ERRORPACKET ep;

   memset(&ep, 0, sizeof(ep));

   ep.opcode = ERROR;
   strncpy(ep.status, gta_status, 2);

   if (msg != NULL)
      strcpy(ep.msg, msg);

   return sendpacket(s, &ep, sizeof(ep));
}
