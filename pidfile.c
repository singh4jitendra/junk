#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>

#include "common.h"

int CreatePIDFile(char *path, char *process_name, char *buffer)
{
   int fd;
   char filename[_POSIX_PATH_MAX+1];
   char pid_data[12];

   sprintf(filename, "%s/%s.pid", path, process_name);
   sprintf(pid_data, "%d\n", getpid());
   if ((fd = creat(filename, READWRITE)) < 0)
      return -1;

   if (write(fd, pid_data, strlen(pid_data)) < 0) {
      close(fd);
      return -1;
   }

   close(fd);

   if (buffer != NULL)
      strcpy(buffer, filename);

   return 0;
}

pid_t ReadPIDFile(char *filename)
{
   pid_t pid = (pid_t)-1;
   FILE *fp;

   if ((fp = fopen(filename, "r")) == NULL)
      return -1;

   fscanf(fp, "%d", &pid);
   fclose(fp);

   return pid;
}
