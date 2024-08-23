#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include "generic.h"

#ifndef ARG_MAX
#define ARG_MAX			_POSIX_ARG_MAX
#endif

void createsnapshot(void)
{
	int i;
	int fd;
	char * f;
	char ** argv;
	int numparms;
	char key[20];
	time_t curtime;
	struct tm * tm;
	char path[PATH_MAX];
	char waitf[PATH_MAX];
	char buffer[ARG_MAX];
	char * cfg = get_cfg_file(NULL, 0);

	curtime = time(NULL);
	tm = localtime(&curtime);

	GetPrivateProfileString(GENSERVER_INI_SECTION,
		GENSERVER_SNAPSHOT_PATH_KEY, path, sizeof(path),
		GENSERVER_DEFAULT_SNAPSHOT_PATH, cfg);
	numparms = GetPrivateProfileInt(GENSERVER_INI_SECTION,
		GENSERVER_SNAPSHOT_ARGS_KEY, GENSERVER_DEFAULT_SNAPSHOT_ARGS, cfg);
	if (numparms == GENSERVER_DEFAULT_SNAPSHOT_ARGS) {
		argv = calloc(10, sizeof(char *));
		argv[0] = "/usr/bin/truss";
		argv[1] = "-pfae";
		argv[2] = "-vall";
		argv[3] = "-X";
		argv[4] = "-rall";
		argv[5] = "-wall";
		argv[6] = "-o";
		argv[7] = calloc(PATH_MAX, sizeof(char));
		strftime(buffer, sizeof(buffer), "%j%H%M%S", tm);
		sprintf(argv[7], "%s/truss.%d.%s", path, getpid(), buffer);
		argv[8] = calloc(10, sizeof(char));
		sprintf(argv[8], "%d", getpid());
		argv[9] = NULL;

		f = argv[7];
	}
	else {
		f = NULL;
		argv = calloc(numparms + 1, sizeof(char *));
		if (argv != NULL) {
			for (i = 0; i < numparms; i++) {
				sprintf(key, "ssarg%d", i + 1);
				GetPrivateProfileString(GENSERVER_INI_SECTION, key, buffer,
					sizeof(buffer), NULL, cfg);
				if (strcmp(buffer, "$pid") == 0) {
					argv[i] = calloc(10, sizeof(char));
					sprintf(argv[i], "%d", getpid());
				}
				else if (strncmp(buffer, "$path", 5) == 0) {
					argv[i] = path;
					if (buffer[5] == ':') {
						strcpy(path, buffer + 6);
					}
				}
				else if (strncmp(buffer, "$file:", 6) == 0) {
					argv[i] = calloc(PATH_MAX, sizeof(char));
					strftime(argv[i], PATH_MAX, buffer + 6, tm);
					f = argv[i];
				}
				else {
					argv[i] = strdup(buffer);
				}
			}
		}
	}

	switch (fork()) {
		case -1:
			break;
		case 0:
			execv(argv[0], argv);
			break;
		default:
			i = 0;
			if (i != NULL) {
				sprintf(waitf, "%s/%s", path, f);
				while ((fd = open(waitf, O_RDONLY)) < 0) {
					sleep(1);
					if (++i > 30) break;
				}
			}

			if (fd >= 0) close(fd);
			for (i = 0; i < numparms; i++) {
				free(argv[i]);
			}

			free(argv);
	}
}
