#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <libgen.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

#include "common.h"
#include "generic.h"
#include "gqconfig.h"
#include "gqxmit.h"
#include "ws_syslog.h"
#include "err.h"
#include "sharemem.h"

#define GQXMIT_OPTIONS "dfi:s"

/* use unsigned ints for time value */
typedef struct {
   u_int32_t seconds;
   u_int32_t hundreds;
} UTIME;

/* structure definitions */
typedef struct {
   char filename[FILENAMELENGTH];
   UTIME filetime;
} USEFILE;

/* function prototypes */
void doforever(void);
void processdirectory(char *);
void soundalarm(int);
void terminate(int);
int findnextfile(char *, ERROR_RETRANSMIT *);

void hp_cheat(void);

/* global variables */
int debuglog;
int shmid;
int alarmed = 0;
GQDATA *prgdata;
char qprefix[FILENAMELENGTH];   /* prefix for queue files              */
char pattern[FILENAMELENGTH];   /* file pattern for file_pattern_match */
char *inifile = WESCOMINI;

int syslogmask = LOG_UPTO(LOG_INFO);

main(int argc, char *argv[])
{
	int option;
	char logname[FILENAMELENGTH];
	mode_t oldmask;
	int flag = 0;
	extern char *optarg;
	int delete_files = -1;

	/* set file creation mask */
	oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

	/* process command line arguments */
	while ((option = getopt(argc, argv, GQXMIT_OPTIONS)) != EOF) {
		switch (option) {
			case 'd':
				syslogmask = LOG_UPTO(LOG_DEBUG);
			case 'f':
				flag = TRUE;
				break;
			case 'i':
				inifile = optarg;
				break;
			case 's':
				delete_files = FALSE;
				break;
			default:
				fprintf(stderr, "usage: GQxmit [%s]\n\n", GQXMIT_OPTIONS);
				exit(1);
		}
	}

	GetPrivateProfileString(GQXMIT_INI_SECTION, GQXMIT_DEBUG_FILE_KEY,
		logname, sizeof(logname), GQXMIT_DEBUG_FILE_DEFAULT, inifile);

	if ((debuglog = creat(logname, READWRITE)) < 0)
		err_sys("can not create debug log %s\n", logname);

	prgdata = (GQDATA *)GetSharedMemory(&shmid, SHM_GQCONFIG, sizeof(GQDATA),
		IPC_CREAT|READWRITE);
	if (!prgdata)
		err_sys("error initializing shared memory");

	hp_cheat();
#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE)
/*
	if (prgdata->pid)
		if (kill(prgdata->pid, SIGWINCH) > -1)
			err_quit("can not have more than one instance of GQxmit\n");
*/
#endif

	prgdata->status = GQ_INITIALIZING;
	prgdata->timeout = GetPrivateProfileInt(GQXMIT_INI_SECTION,
		GQXMIT_TIMEOUT_KEY, GQXMIT_TIMEOUT_DEFAULT, inifile);
	prgdata->maxretries = GetPrivateProfileInt(GQXMIT_INI_SECTION,
		GQXMIT_MAX_RETRIES_KEY, GQXMIT_MAX_RETRIES_DEFAULT, inifile);
	prgdata->mailedmsgs = 0;
	if (delete_files == -1) {
		prgdata->deletefiles= GetPrivateProfileInt(GQXMIT_INI_SECTION,
			GQXMIT_DELETE_FILES_KEY, TRUE, inifile);
	}

	prgdata->debug = flag;
	GetPrivateProfileString(GQXMIT_INI_SECTION, GQXMIT_MAILTO_KEY,
		prgdata->mailto, sizeof(prgdata->mailto), GQXMIT_MAILTO_DEFAULT, inifile);

	daemoninit();

	prgdata->pid = getpid();

	ws_openlog("GQxmit", LOG_PID, LOG_LOCAL7, syslogmask);

	doforever();

	prgdata->status = GQ_TERMINATED;
	prgdata->pid = 0;
	shmdt(prgdata);
	umask(oldmask);

	return 0;
}

void doforever(void)
{
	DIR *dir;
	struct dirent *entry;
	struct sigaction alrmact;
	struct sigaction usr1act;
	sigset_t process_mask;
	char queuedir[FILENAMELENGTH];
	char subdirname[FILENAMELENGTH];
	int pos;
	struct stat fs;

	/* set up SIGALRM handler */
	alrmact.sa_handler = soundalarm;
	sigemptyset(&alrmact.sa_mask);
	sigaddset(&alrmact.sa_mask, SIGHUP);
	sigaddset(&alrmact.sa_mask, SIGUSR1);
	alrmact.sa_flags = 0;
	sigaction(SIGALRM, &alrmact, NULL);

	/* set up SIGUSR1 handler */
	usr1act.sa_handler = terminate;
	sigemptyset(&usr1act.sa_mask);
	sigaddset(&usr1act.sa_mask, SIGALRM);
	sigaddset(&usr1act.sa_mask, SIGHUP);
	usr1act.sa_flags = 0;
	sigaction(SIGUSR1, &usr1act, NULL);

	sigemptyset(&process_mask);
	sigaddset(&process_mask, SIGHUP);
	sigaddset(&process_mask, SIGPIPE);
	sigprocmask(SIG_SETMASK, &process_mask, (sigset_t *)NULL);

	/* read INI file */
	GetPrivateProfileString(GENSERVER_INI_SECTION, "queuedir", queuedir,
		sizeof(queuedir), "/IMOSDATA/2/QUEUES/GQDIR", inifile);
	GetPrivateProfileString(GENSERVER_INI_SECTION, GENSERVER_QPREFIX_KEY,
		qprefix, sizeof(qprefix), GENSERVER_DEFAULT_QPREFIX, inifile);

	/* set pattern matching string */
	sprintf(pattern, "%s*", qprefix);

	/* remove trailing '/' if necessary */
	if (*(queuedir + strlen(queuedir) - 1) == '/')
		*(queuedir + strlen(queuedir) - 1) = 0;

	/* log initialization information to debug log */
	if (prgdata->debug) {
		wescolog(debuglog, "queueing directory = %s\n", queuedir);
		wescolog(debuglog, "queue name prefix = %s\n", qprefix);
		wescolog(debuglog, "pattern matching = %s\n", pattern);
	}

	prgdata->status = GQ_RUNNING;

	CheckDirectory(queuedir);

	while (1) {
		if (prgdata->status == GQ_SHUTTINGDOWN)
			return;

		sleep(5);

		heartbeat();

		if (prgdata->status == GQ_SHUTTINGDOWN)
			return;

		if (stat(queuedir, &fs) == -1) {
			wescolog(NETWORKLOG,	"%s: can not status %s.",
				GQXMIT_INI_SECTION, queuedir);
			continue;
		}

		if (!S_ISDIR(fs.st_mode))
			continue;

		/* open the queue root directory */
		if ((dir = opendir(queuedir)) == NULL) {
			wescolog(NETWORKLOG,	"%s: can not open the directory %s!",
				GQXMIT_INI_SECTION, queuedir);
			continue;
		}

		/*
			Each entry in the queue root directory should be a directory
			that corresponds to a known host.  Open the directory and
			process it.
		*/
		while ((entry = readdir(dir)) != NULL) {
			if (prgdata->status == GQ_SHUTTINGDOWN) {
				closedir(dir);
				return;
			}

			/* get the queue root directory name */
			strcpy(subdirname, queuedir);
			pos = strlen(subdirname);
			if (subdirname[pos-1] != '/')
				strcat(subdirname, "/");

			/* append name of directory to process */
			strcat(subdirname, entry->d_name);

			if (prgdata->debug)
				wescolog(debuglog, "Processing %s\n", subdirname);

			if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
				processdirectory(subdirname);
		}

		closedir(dir);
	}
}

void processdirectory(char *thedirname)
{
	int qfd;
	int errorfile;
	int status;
	struct dirent *entry;
	struct stat fs;
	GENTRANSAREA parameters;
	char *temp;
	char pattern[FILENAMELENGTH];
	char errmsg[25];
	char errdesc[25];
	ERROR_RETRANSMIT fileinfo;
	char errfilename[FILENAMELENGTH];

	memset(&fileinfo, 0, sizeof(fileinfo));

	if (findnextfile(thedirname, &fileinfo) < 0)
		return;

	if (prgdata->debug)
		wescolog(debuglog, "getting status of %s\n", fileinfo.filename);

	/* check to see if the file is the right size */
	if (stat(fileinfo.filename, &fs) >= 0) {
		if (fs.st_size < sizeof(parameters)) {
			if (prgdata->debug)
				wescolog(debuglog, "File size is %d, it should be %d\n",
					fs.st_size, sizeof(parameters));
			unlink(fileinfo.filename);
			return;
		}

		if (prgdata->debug)
			wescolog(debuglog, "opening file %s\n", fileinfo.filename);

		if ((qfd = open(fileinfo.filename, O_RDONLY)) < 0) {
			if (prgdata->debug)
				wescolog(debuglog, "error opening %s - errno %d", fileinfo.filename,
					errno);

			return;
		}

		if (prgdata->debug)
			wescolog(debuglog, "reading %d bytes\n", sizeof(parameters));

		if (read(qfd, &parameters, sizeof(parameters)) < sizeof(parameters)) {
			if (prgdata->debug)
				wescolog(debuglog, "error reading file - errno %d", errno);

			close(qfd);
			return;
		}

		close(qfd);

		if (prgdata->debug)
			wescolog(debuglog, "calling WDOCGENERIC()\n");

		WDOCGENERIC(&parameters);

		if (prgdata->debug)
			wescolog(debuglog, "returned from WDOCGENERIC()\n");

		if (isdigit(parameters.status[0]) && isdigit(parameters.status[1])) {
			status = (parameters.status[0] - '0') * 10;
			status += parameters.status[1] - '0';
		}
		else {
			parameters.status[0] = '9';
			parameters.status[1] = '9';
			status = 99;
		}

		if (prgdata->debug) {
			wescolog(debuglog, "status = %d\n", status);
		}

		if (status != 0) {
			memset(errmsg, 0, sizeof(errmsg));
			memset(errdesc, 0, sizeof(errdesc));

			CobolToC(errmsg, parameters.error.name, sizeof(errmsg),
				sizeof(parameters.error.name));
			CobolToC(errdesc, parameters.error.description, sizeof(errdesc),
				sizeof(parameters.error.description));

			if (prgdata->debug) {
				wescolog(debuglog, "error name = %s\n", errmsg);
				wescolog(debuglog, "error description = %s\n", errdesc);
			}

				ws_syslog(LOG_ERR, "GTA-RET-CODE - %02d: %s: %s", status, errmsg,
					errdesc);
		}

		if (status == 0) {
			if (prgdata->deletefiles)
				remove_by_pattern(fileinfo.filename);
/*				unlink(fileinfo.filename); */
			else {
				strcpy(errfilename, fileinfo.filename);
				strcat(errfilename, ".save");
				rename(fileinfo.filename, errfilename);
			}

			sprintf(errfilename, "%s/error.retransmit", thedirname);
			unlink(errfilename);
		}
		else if (status >= 20) {
			fileinfo.retries++;
			if (fileinfo.retries >= prgdata->maxretries) {
				sprintf(errfilename, "%s/error.retransmit", thedirname);
				unlink(errfilename);
				if ((temp = strdup(fileinfo.filename)) != NULL) {
					sprintf(errfilename, "%s/error.%s", thedirname, basename(temp));
					rename(fileinfo.filename, errfilename);
					free(temp);
				}
			}
			else {
				sprintf(errfilename, "%s/error.retransmit", thedirname);
				if ((errorfile = open(errfilename, O_CREAT|O_WRONLY, 0600)) > -1) {
					write(errorfile, &fileinfo, sizeof(fileinfo));
					close(errorfile);
				}
			}
		}
	}
	else if (prgdata->debug)
		wescolog(debuglog, "error getting status of %s - errno %d\n",
			fileinfo.filename, errno);

	return;
}

/* ARGSUSED */
void soundalarm(int status)
{
	alarmed = TRUE;
}

/* ARGSUSED */
void terminate(int status)
{
	prgdata->status = GQ_SHUTTINGDOWN;
}

int findnextfile(char *thedirname, ERROR_RETRANSMIT *fileinfo)
{
	DIR *dir;
	struct dirent *entry;
	int error_number;
	char *period;
	USEFILE queuefile;
	UTIME filetime;
	int errorfile;
	char fulldirname[FILENAMELENGTH];

	sprintf(fulldirname, "%s/error.retransmit", thedirname);
	if ((errorfile = open(fulldirname, O_RDONLY)) > -1) {
		if (read(errorfile, fileinfo, sizeof(ERROR_RETRANSMIT)) ==
			 sizeof(ERROR_RETRANSMIT)) {
			close(errorfile);
			return 0;
		}
		else
			close(errorfile);
	}

	memset(fileinfo, 0, sizeof(ERROR_RETRANSMIT));

	memset(queuefile.filename, 0, sizeof(queuefile.filename));
	queuefile.filetime.seconds = UINT_MAX;
	queuefile.filetime.hundreds = UINT_MAX;

	if (prgdata->debug)
		wescolog(debuglog, "searching for next file\n");

	/* open queue directory for reading */
	if ((dir = opendir(thedirname)) == NULL) {
		if (prgdata->debug) {
			error_number = errno;
			wescolog(debuglog, "Error opening directory: %s %d - %s\n",
				 thedirname, error_number, strerror(error_number));
		}

		return -1;
	}

	/* loop for every entry in the directory */
	while ((entry = readdir(dir)) != NULL) {
		if (file_pattern_match(entry->d_name, "genqueue.*.gendata")) {
			if ((period = strchr(entry->d_name, '.')) != NULL) {
				filetime.seconds = strtoul(period + 1, &period, 10);
				filetime.hundreds = strtoul(period + 1, &period, 10);

				if ((filetime.seconds < queuefile.filetime.seconds) ||
					((filetime.seconds == queuefile.filetime.seconds) &&
					 (filetime.hundreds < queuefile.filetime.hundreds))) {
					queuefile.filetime = filetime;
					strcpy(queuefile.filename, thedirname);
					strcat(queuefile.filename, "/");
					strcat(queuefile.filename, entry->d_name);
				}
			}
		}
	}

	strcpy(fileinfo->filename, queuefile.filename);
	closedir(dir);

	return 0;
}

int remove_by_pattern(char * filename)
{
	char the_dir_name[PATH_MAX + 1];
	char pattern[PATH_MAX + 1];
	char full_name[PATH_MAX + 1];
	char * last_slash;
	char * last_dot;
	DIR * dir;
	struct dirent * entry;

	memset(the_dir_name, 0, sizeof(the_dir_name));
	memset(pattern, 0, sizeof(pattern));

	if ((last_slash = strrchr(filename, '/')) == NULL) {
		strcpy(the_dir_name, ".");
		last_slash = filename - 1;
	}
	else {
		if (last_slash == filename) {
			*the_dir_name = *filename;
		}
		else {
			strncpy(the_dir_name, filename, (size_t)(last_slash - filename));
		}
	}

	if ((last_dot = strrchr(filename, '.')) == NULL) {
		strcpy(pattern, last_slash + 1);
	}
	else {
		if (last_dot < last_slash) {
			strcpy(pattern, last_slash + 1);
		}
		else {
			strncpy(pattern, last_slash + 1, (size_t)(last_dot - last_slash));
		}
	}

	strcat(pattern, "*");

	if ((dir = opendir(the_dir_name)) == (DIR *)NULL) {
		return -1;
	}

	while ((entry = readdir(dir)) != (struct dirent *)NULL) {
		if (file_pattern_match(entry->d_name, pattern)) {
			sprintf(full_name, "%s/%s", the_dir_name, entry->d_name);
			remove(full_name);
		}
	}

	closedir(dir);
}

void heartbeat(void)
{
	static int pulse = 0;
	time_t curtime;
	struct tm * ptr;

	curtime = time(NULL);
	ptr = localtime(&curtime);

	if ((ptr->tm_min % 30) == 0) {
		if (pulse == 0) {
			ws_syslog(LOG_INFO, "heartbeat to avoid false nims reports");
			pulse = 1;
		}
	}
	else {
		pulse = 0;
	}

	return;
}

void hp_cheat(void)
{
	int fd_master, fd_slave;
	char slave_name[256];

	fd_master = ptym_open(slave_name);
	fd_slave = ptys_open(fd_master, slave_name);

	close(0);
	dup2(fd_master, 0);
	close(1);
	dup2(fd_master, 1);
	close(2);
	dup2(fd_master, 2);

	close(fd_master);
}
