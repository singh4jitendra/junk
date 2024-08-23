#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#if !defined(NO_SEMAPHORE)
#   define ALLOWED_TO_CREATE_SEMAPHORE
#endif

#if !defined(NO_DIRECTORY)
#   define MAKE_DIRECTORY
#endif

#define STATUS_OK    0
#define STATUS_ERROR 1
#define STATUS_FULL  2

#define EXIT_ERROR(x,y)   {\
                             fprintf(stderr, "%s\n", x);\
                             return y;\
                          }

#define EXIT_PERROR(x,y)  {\
                             perror("nextpart");\
                             EXIT_ERROR(x,y);\
                          }

main(int argc, char *argv[])
{
   int seminit[2] = { 0, 0 };
   int num, *tmp, numinuse[1000], semid, count;
   char msg[300];
#if defined(MAKE_DIRECTORY)
   char partdir[10];
   mode_t oldmask;
#endif
   char *userdir;
   char *suffix;
#if defined(USENVIRONMENT)
   char *envrtmp;
#endif
   DIR *dir;
   key_t key;
   int flags;
   struct dirent *entry;
   struct sembuf lockdir[] = {
      { 0, 0, SEM_UNDO },    /* wait for semaphore #0 == 0  */
      { 0, 1, SEM_UNDO }     /* increment semaphore #0 by 1 */
   };

   /* initialize memory */
   memset(numinuse, 0, sizeof(numinuse));

#if defined(MAKE_DIRECTORY)
   oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));
#endif

#if defined(USENVIRONMENT)
   /* look up environmental variable */
   if ((envrtmp = getenv("userdir")) != NULL)
      userdir = envrtmp;
#else
   /* user directory should be specified as a command line parameter */
   if (argc < 3)
      EXIT_ERROR("usage: nextpart <user directory> <suffix>", STATUS_ERROR);

   userdir = argv[1];
   suffix = argv[2];
#endif

#if defined(ALLOWED_TO_CREATE_SEMAPHORE)
   /* get the semaphore token */
#if !defined(__OPENNT)
   if ((key = ftok(userdir, 25)) < 0) {
      sprintf(msg, "error reading %s", userdir);
      EXIT_ERROR(msg, STATUS_ERROR);
   }
#else
   key = ('W' << 8) + 32;
#endif

   /* get the semaphore id */
   flags = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
   if ((semid = semget(key, 1, flags)) < 0) {
      flags |= IPC_CREAT;
      if ((semid = semget(key, 1, flags)) < 0)
         EXIT_PERROR("error retrieving semaphore", STATUS_ERROR);

      if (semctl(semid, 0, SETALL, seminit) < 0)
         EXIT_PERROR("error initializing semaphore", STATUS_ERROR);
   }

   if ((semop(semid, lockdir, 2)) < 0)
      EXIT_PERROR("error performing semaphore operation", STATUS_ERROR);
#endif

   /* open the directory */
   if ((dir = opendir(userdir)) == NULL) {
      semctl(semid, 0, SETVAL, 0);
      sprintf(msg, "error opening %s", userdir);
      EXIT_PERROR(msg, STATUS_ERROR);
   }

   /* read every entry in directory; get partition number from entry */
   tmp = numinuse;
   while ((entry = readdir(dir)) != NULL) {
      if (sscanf(entry->d_name, "%003d", &num))
         *(tmp+num) = 1;
      else if (sscanf(entry->d_name, "gp%003d", &num))
         *(tmp+num) = 1;
   }

   closedir(dir);

   /* search for first open number */
   for (count = 0; count < 1000; count++)
      if (!(*(tmp+count)))
         break;

#if defined(MAKE_DIRECTORY)
   sprintf(partdir, "%003d%s", count, suffix);

   if (chdir(userdir))
      EXIT_PERROR("could not change directory", STATUS_ERROR);

   if (mkdir(partdir, S_IRWXU|S_IRWXG|S_IRWXO))
      EXIT_PERROR("could not create directory", STATUS_ERROR);

#if !defined(_OPENNT)
   if (chown(partdir, getuid(), getgid()))
      EXIT_PERROR("could not change ownership", STATUS_ERROR);
#endif

   umask(oldmask);
#endif

#if defined(ALLOWED_TO_CREATE_SEMAPHORE)
   semctl(semid, 0, SETVAL, 0);
#endif

   printf("%003d\n", count);

   return (count < 1000) ? STATUS_OK : STATUS_FULL;
}
