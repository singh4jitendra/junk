#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int check_pid_file(char *filename)
{
	int fd;
	pid_t pid;
	FILE *pid_file_fp;
	struct flock file_lock = { F_WRLCK, SEEK_SET, 0, 0 };

	/* try to create the file, fail if it exists */
	fd = open(filename, O_RDWR|O_CREAT|O_EXCL,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if (fd < 0) {                  /* an error occurred creating the file */
		if (errno == EEXIST) {      /* file exists */
			if ((pid_file_fp = fopen(filename, "r+")) == (FILE *)NULL)
				return -1;

			fd = fileno(pid_file_fp);

			/* set a write lock to prevent another instance of the program
				from accessing the file while we are determining if we should
				allow this instance to run as the daemon */
			if (fcntl(fd, F_SETLKW, &file_lock) < 0) {
				fclose(pid_file_fp);
				return -1;
			}

			if (fscanf(pid_file_fp, "%d", &pid) < 1) {
				file_lock.l_type = F_UNLCK;
				fcntl(fd, F_SETLK, &file_lock);
				fclose(pid_file_fp);
				return -1;
			}

			if (!kill(pid, SIGWINCH)) {
				/* signal sent, another server is running */
				file_lock.l_type = F_UNLCK;
				fcntl(fd, F_SETLK, &file_lock);
				fclose(pid_file_fp);
				return -1;
			}
			else {
				/* error occurred sending the signal */
				if (errno == ESRCH) {  /* process id does not exist */
					/* seek to the start of the file and write our pid*/
					fseek(pid_file_fp, 0, SEEK_SET);
					fprintf(pid_file_fp, "%d\n", getpid());
					fflush(pid_file_fp);

					/* release the lock on the file */
					file_lock.l_type = F_UNLCK;
					fcntl(fd, F_SETLK, &file_lock);

					fclose(pid_file_fp);
				}
			}
		}
	}
	else {                  /* pid file created successfully */
		if (fcntl(fd, F_SETLKW, &file_lock) < 0) {
			close(fd);
			return -1;
		}

		if ((pid_file_fp = fdopen(fd, "r+")) == (FILE *)NULL) {
			close(fd);
			return -1;
		}

		fprintf(pid_file_fp, "%d\n", getpid());
		fflush(pid_file_fp);

		file_lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &file_lock);

		fclose(pid_file_fp);
		close(fd);
	}

	return 0;
}
