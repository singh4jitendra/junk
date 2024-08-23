#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <stdio.h>

main(int argc, char *argv[])
{
   char *filename;
   int additive;
   key_t key;
   int flags;
   int semid;
   int seminit[2] = { 0, 0};

   if (argc < 3) {
      fprintf(stderr,
         "Usage: createsem <filename> <additive>\n");
      exit(1);
   }

   filename = argv[1];
   additive = atol(argv[2]);

   if ((key = ftok(filename, additive)) < 0) {
      perror("createsem");
      fprintf(stderr, "createsem: error on file %s\n", filename);
      exit(1);
   }

   /* get the semaphore id */
   flags = IPC_CREAT|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
   if ((semid = semget(key, 1, flags)) < 0) {
      perror("createsem");
      fprintf(stderr, "createsem: error creating semaphore\n");
      exit(1);
   }

   if (semctl(semid, 0, SETALL, seminit) < 0) {
      perror("createsem");
      fprintf(stderr, "createsem: error initializing semaphore\n");
      exit(1);
   }
}
