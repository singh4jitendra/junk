
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <grp.h>
#include <pwd.h>

int gs_get_file_info(int, mode_t *mode, char *, size_t, char *, size_t);

int main(int argc, char * argv[])
{
	int i;
	int fd;
	int ret;
	mode_t m;
	char owner[9];
	char group[9];
	size_t ownersize;
	size_t groupsize;

	if (argc < 2) {
		fprintf(stderr, "usage: gfi file_name ...\n\n");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		if (fd != -1) {
			/* get all info */
			ownersize = sizeof(owner);
			groupsize = sizeof(group);
			ret = gs_get_file_info(fd, &m, owner, ownersize, group, groupsize);
			printf("%-40s: mode: %06o %-8s %-8s\n", argv[i], m, owner, group);

			/* get owner & group */
			ownersize = sizeof(owner);
			groupsize = sizeof(group);
			ret = gs_get_file_info(fd, NULL, owner, ownersize, group, groupsize);
			printf("%-40s: mode: ------ %-8s %-8s\n", argv[i], owner, group);

			/* get mode & group */
			ownersize = 0;
			groupsize = sizeof(group);
			ret = gs_get_file_info(fd, &m, NULL, ownersize, group, groupsize);
			printf("%-40s: mode: %06o -------- %-8s\n", argv[i], m, group);

			/* get mode & owner */
			ownersize = sizeof(owner);
			groupsize = 0;
			ret = gs_get_file_info(fd, &m, owner, ownersize, NULL, groupsize);
			printf("%-40s: mode: %06o %-8s --------\n", argv[i], m, owner);

			close(fd);
		}
		else {
			perror("main");
		}
	}
}

int get_file_info(int fd, mode_t * mode, char * owner, size_t ownersize,
	char * group, size_t groupsize)
{
	struct group *ge;
	struct passwd * pe;
	struct stat fs;
	size_t length;

	if (fstat(fd, &fs) == -1) {
		perror("get_file_info");
		return -1;
	}

	if (mode != NULL)
		*mode = fs.st_mode;

	if ((owner != NULL) && (ownersize > 1)) {
		pe = getpwuid(fs.st_uid);
		if (pe == NULL)
			sprintf(owner, "%d", fs.st_uid);
		else {
			length = strlen(pe->pw_name);
			if (length >= ownersize) {
				strncpy(owner, pe->pw_name, ownersize - 1);
				*(owner + (ownersize - 1)) = 0;
			}
			else
				strcpy(owner, pe->pw_name);
		}
	}

	if ((group != NULL) && (groupsize > 1)) {
		ge = getgrgid(fs.st_gid);
		if (ge == NULL)
			sprintf(group, "%d", ge->gr_gid);
		else {
			length = strlen(ge->gr_name);
			if (length >= groupsize) {
				strncpy(group, ge->gr_name, groupsize - 1);
				*(group + (groupsize - 1)) = 0;
			}
			else
				strcpy(group, ge->gr_name);
		}
	}
}
