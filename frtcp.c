/***********************************************************************
 *
 * frtcp.c - This program will be used to talk to the Tandem via
 *       TCP/IP and place the files that come across into the
 *       fwhs directory.  Then the proc program can pick the
 *       files up there to pass along to the queues.
 *
 * Compiling instructions ...
 *    cc frtcp.c -ofrtcp -O3 -Hansi -s -lsocket -lnsl -lresolv -lx
 *
 * J. Shaver - G. T. Systems - 01/07/97
 *
 * J. Shaver - 06/27/97 - For new operating system, need to retry the
 *                        bind more than once, since it doesn't let go
 *                        of the process the same way.
 *
 * J. Shaver - 10/19/01 - Now that they're on a new system, can accept
 *                        more than 500 file AND not sleep so long!
 *********************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/fcntl.h>
#include <string.h>

#ifndef _GNU_SOURCE
#include <sys/locking.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#ifndef _GNU_SOURCE 
#include <sys/flock.h>
#endif

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>

#define INET_ERROR 4294967295 /* inet addr return 0xffffffff1 upon error */
#define EOFmark    26   /* Hex 1A - End of file  */
#define EOTmark    04   /* Hex 04 - End of transmission */
#define OK        "OK"  /* Send OK back to client */
/* #define MAXDIRENTS 500    Maximum number of files before we say there's
                          a problem somewhere */

#define MAXDIRENTS 5000  /* Maximum number of files before we say there's
                            a problem somewhere */



/************************* Global Variables *********************************/

struct hostent *hp, *clientent;
struct servent *sp;

struct sockaddr_in myaddr_in, peeraddr_in;

char trancode[4], seqname[100], seqFilename[100], seqNumber[10];
char srcFilename[100], destDir[100], destFilename[100], servname[30];
char whsdir[256], QPath[100], myname[81], clientname[81];
char connectname[81], ok[3];
int s, ls, openfile, noappend, filenum, invalidconn;
int major_count=0, numentries=0, nextNumber, seqFile;
int32_t numconnect = 0;


/************************ Function Prototypes *******************************/

char *getenv();
void checkparams(int argc, char *argv[]);
void getvalidconn(void);
int  frtcp(void);
int  catchsig(int);
void GetFilename(char *, char *);
int  countdirentries(char *);


/*********************** Main Program ****************************************/

main (argc, argv)
int argc;
char *argv[];
{

   int retcode, addrLen, num, nummsg, i, bindcnt, bindsw;

   /* Declare signal catching function */
   int catchsig();


   checkparams(argc, argv);

   getvalidconn();


   openfile=0;
   invalidconn=0;
   nummsg = 0;
   bindcnt=0;

   /* Associate signals to be caught with function */
   signal(SIGINT, catchsig);
   signal(SIGUSR1, catchsig);


   /* Let's count files before we accept a connection to make sure
      we can accept more from the other machine
   */
   countfiles:
 
   while ((num=countdirentries(whsdir)) > MAXDIRENTS)  {
      if (nummsg < 20)  {
         nummsg++;
         fprintf(stderr, "Too many files!!\nPlease investigate.\n");
         fflush(stderr);
      }
      sleep(5);
   }


   if (num < 0)  {
      close(ls);
      fprintf(stderr, "Countdirent had problem (-1)\n");
      fflush(stderr);
      exit(1);
   }

   fprintf(stderr, "Number of files OK ...\nContinuing\n");
   fflush(stderr);

   nummsg=0;


   memset((char *)&myaddr_in,0,sizeof(struct sockaddr_in));
   memset((char *)&peeraddr_in,0,sizeof(struct sockaddr_in));

   myaddr_in.sin_addr.s_addr = INADDR_ANY;
   myaddr_in.sin_family = AF_INET;

   /* Set the port number from the services file */
   if ((sp = getservbyname(servname, "tcp")) == NULL)  {
      fprintf(stderr, "SERVER: requested service not on file\n");
      exit(1);
   }

   myaddr_in.sin_port = sp->s_port;

   beginning:
   bindcnt=0;
   bindsw=0;
   /* Let's open the socket  */
   if ((ls = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
      perror("SERVER: Socket open failed ");
      exit(1);
   }

   bindagain:
   /* Bind the socket */
   if (bind(ls, (struct sockaddr *)&myaddr_in, sizeof(struct sockaddr_in)) < 0)  {
      bindcnt++;
      bindsw=1;
      perror("SERVER: Bind failed ");
      if (errno == EADDRINUSE)  {
        if (bindcnt < 10)  {
            sleep(5);
            goto bindagain;
        }
      }
      exit(1);
   }

   if (bindsw)  {
      bindsw=0;
      fprintf(stderr,"Bind OK\n");
   }


   /* Listen to the port - 5 is the maximum number of sockets
      in the listen buffer at a given time.  Once a conversation
      is accepted, it's spot in the listen buffer is freed.
   */
   if (listen(ls, 5) < 0)  {
      perror("SERVER: Listen failed ");
      exit(1);
   }


   addrLen = sizeof(struct sockaddr_in);

   startagain:

   if ((s = accept(ls, (struct sockaddr *)&peeraddr_in, &addrLen)) < 0)  {
      perror("SERVER: Accept failed ");
      exit(1);
   }


   clientent = gethostbyaddr((char *)&peeraddr_in.sin_addr.s_addr,
                  sizeof(peeraddr_in.sin_addr.s_addr), AF_INET);

   for (i=0; i<10; i++)
      connectname[i]=clientent->h_name[i];
   connectname[i]=0;

   if (strcmp(clientname, connectname) != 0)  {
      if (invalidconn < 11)  {
         invalidconn++;
         fprintf(stderr, "Invalid client coming in!!\n");
         fprintf(stderr, "  Closing connection\n");
         fflush(stderr);
      }
      close(s);
      goto startagain;
   }

   invalidconn = 0;

   while (1)  {
      retcode = frtcp();
      if (retcode)  {
         close(s);
         if (retcode < 0)  {
            close(ls);
            goto countfiles;
         }
         else
            goto startagain;
      }
      sleep(2);
   }

}


/****************************************************************************/

int frtcp(void)
{
   int bytes, retcode, numtries, done, pos, senderr, num, nummsg;
   char buff[600], *EofPos, outname[100];
   char *foundend, transcode[4];
   DIR *trandir;

   senderr=0;

   while (1)  {
      numtries=0;
      bytes = ReceiveData(s, buff, 12, 0);
      if (bytes == -1 || bytes == 0)  {
         return(1);
      }

      if (strncmp(buff, "$$QSTX", 6)  == 0)  {
         transcode[0] = buff[6];
         transcode[1] = buff[7];
         transcode[2] = buff[8];
         transcode[3] = 0;

         strcpy(destDir, whsdir);
         strcat(destDir, "/");

         strcpy(srcFilename, destDir);

         if (!noappend)  {
            strcat(destDir, transcode);
            strcpy(outname, destDir);
            strcat(outname, "/");
            strcat(destDir, "/");
         }
         else
            strcpy(outname, destDir);

         while (numtries < 20)  {
            destFilename[0]=0;
            GetFilename(QPath, destFilename);
            strcat(srcFilename, destFilename);
            if ((filenum = open(srcFilename, O_CREAT|O_WRONLY)) == NULL)  {
               fprintf(stderr, "Can't open file - %s\n", srcFilename);
               fflush(stderr);
               numtries++;
            }
            else
               break;
         }
         if (numtries >= 20)  {
            fprintf(stderr, "Closing Connection...\nPlease Investigate!!\n");
            fflush(stderr);
            close(s);
            exit(1);
         }


         numtries = 0;
         openfile = 1;
         done = 0;

         while (!done)  {
            memset(buff, 0, 513);
            bytes = ReceiveData(s, buff, 512, 0);
            if (bytes == -1 || bytes == 0)  {
               fprintf(stderr, "Problem getting data - %d\n", bytes);
               fprintf(stderr, " Closing file and connection!\n");
               fflush(stderr);
               close(filenum);
               unlink(srcFilename);
               openfile=0;
               return(1);
            }

            for (EofPos=buff, pos=0; pos < 512 && *EofPos != EOFmark;
                 pos++, EofPos++);

            if (pos < 512)
               done = 1;
            else
               pos = 512;
            write(filenum, buff, pos);
         }

         if ((retcode=send(s, ok, 2, 0)) != 2)  {
            if (retcode < 0)
               perror("Send OK failed ");
            fprintf(stderr, "Send OK message error!\n");
            fprintf(stderr, " Closing socket and continuing\n");
            fflush(stderr);
            senderr=1;
         }

         strcat(outname, destFilename);

         chmod(srcFilename, S_IRWXU | S_IRWXG | S_IRWXO);


         close(filenum);

         retcode = rename(srcFilename, outname);
         if (retcode < 0)  {
            perror("Can't rename!");
            fprintf(stderr,"SrcFilename '%s'\n", srcFilename);
            fprintf(stderr, "Outname '%s'\n", outname);

         }
         else  {
            numentries++;
         }

         openfile = 0;
         if (numentries > MAXDIRENTS)  {
            numentries = 0;
            return(-1);
         }
         if (senderr)
            return(1);
      } /* end if QSTX */
   } /* end while forever */
}

/****************************************************************************/


void GetFilename(char *QPath, char *destFilename)
{

   lseek(seqFile, 0, 0);
   if (lockf(seqFile, F_TLOCK, 0) == -1)  {
      fprintf(stderr,"Can't lock seq file!!\n");
      fprintf(stderr,"Process aborted!!\n");
      perror("Seq file ");
      exit(1);
   }

   read(seqFile, seqNumber, 6);
   seqNumber[6] = 0;

   strcat(destFilename, seqNumber);
   sscanf(seqNumber, "%6uld", &nextNumber);
   if ((++nextNumber) > 999999)
      nextNumber = 0;

   sprintf(seqNumber, "%000006uld", nextNumber);
   lseek(seqFile, 0, 0);
   write(seqFile, seqNumber, 6);
   lseek(seqFile, 0, 0);
   lockf(seqFile, F_ULOCK, 0);


   return;
}


/**************************************************************************/
/*  Function to handle SIGINT and SIGUSR1                                 */
/**************************************************************************/

catchsig(signo)
int signo;
{
   char outfile[100];

   if (openfile)  {
      close(filenum);
      strcpy(outfile, srcFilename);
      strcat(outfile, ".err");
      rename(srcFilename, outfile);
      chmod(outfile, S_IRWXU | S_IRWXG | S_IRWXO);
      fprintf(stderr, "Renamed file %s\n", outfile);
      fflush(stderr);
   }

   close(seqFile);
   close(s);
   close(ls);
   fprintf(stderr, "\nCATCHSIG: signo=%d\n", signo);
   exit(0);
}

/***************************************************************************/

void checkparams(argc, argv)
int  argc;
char *argv[];
{
   char *tempdir, whsbuff[100];
   struct stat statbuff;
   struct dirent *holdent;
   DIR *curdir;
   int position;

   if (argc < 3 || argc > 4)  {
      printf("Usage is : %s hostname servname whsdir\n", argv[0]);
      printf("  Where hostname is the TCP host to connect to \n");
      printf("  servname is the name in the services file to use \n");
      printf("  whsdir is the directory where the files are to be put\n");
      exit(1);
   }

   strcpy(servname, argv[2]);

   if (argc == 4)
      strcpy(whsdir, argv[3]);
   else
      if ((tempdir = getenv("QTRANS")) == NULL)  {
         perror("Error - No whs directory");
         exit(1);
      }
      else  {
         strcpy(whsdir, tempdir);
         strcat(whsdir, "/fwhs");
      }

   if (whsdir[strlen(whsdir)-1]=='/') whsdir[strlen(whsdir)-1]='\0';

   noappend=1;
   if ((curdir = opendir(whsdir)) == NULL)  {
      perror("Invalid whsdir! ");
      exit(1);
   }

   rewinddir(curdir);

   while ((holdent = readdir(curdir)) != NULL)  {
      if (holdent->d_name[0] != '.')  {
         strcpy(whsbuff, whsdir);
         strcat(whsbuff, "/");
         strcat(whsbuff, holdent->d_name);
         if ((stat(whsbuff, &statbuff)) < 0)  {
            perror("Stat error! ");
            exit(1);
         }
         if (statbuff.st_mode & S_IFDIR)
            noappend=0;
      }
   }

   closedir(curdir);

   if ((tempdir = getenv("QDATA")) == NULL)  {
      perror("Error - No QDATA set!\n");
      exit(1);
   }

   strcpy(QPath, tempdir);

   strcpy(ok, OK);


   memset(seqFilename, 0, 64);
   memset(seqNumber, 0, 7);

   strcpy(seqFilename, QPath);
   position = strlen(seqFilename) - 1;
   if (seqFilename[position] != '/')  {
      seqFilename[position+1]='/';
      seqFilename[position+2]=0;
   }
   strcat(seqFilename, "frseq");

   while (1)  {
      if ((seqFile = open(seqFilename, O_RDWR)) == -1)  {
         seqFile = open(seqFilename, O_WRONLY | O_CREAT);
         nextNumber = 1;
         sscanf(seqNumber, "%6uld", &nextNumber);

         write(seqFile, seqNumber, 6);
         close(seqFile);
      }
      else
         break;
   }
   return;
}


/****************************************************************************/

int ReceiveData(int s, char *buffer, int bufSize, int flags)
{
   int totalBytes = 0, bytesReceived = 0;
   int retcode;

   totalBytes = recv(s, buffer, bufSize, flags);
   if ((totalBytes == -1) || (totalBytes == 0))  {
      if (totalBytes == -1)
         perror("RECV error ");
      return(totalBytes);
   }

   if (buffer[totalBytes-1] == EOFmark)  {
      return(totalBytes);
   }
   if (buffer[totalBytes-1] == EOTmark)   {
      fprintf(stderr, "Client sent EOT error\n");
      fprintf(stderr, "Treat like TCP error\n");
      fflush(stderr);
      return(-1);
   }


   while (totalBytes < bufSize)  {
      bytesReceived = recv(s, buffer+totalBytes, bufSize-totalBytes,
                           flags);
      if (bytesReceived == -1)  {
         perror("RECV error ");
         return(bytesReceived);
      }
      if (bytesReceived == 0)
         return(bytesReceived);


      totalBytes += bytesReceived;

      if (buffer[totalBytes-1] == EOFmark)  {
         break;
      }
      if (buffer[totalBytes-1] == EOTmark)  {
         fprintf(stderr,"Client sent EOT error\n");
         fprintf(stderr, "Treat like TCP error\n");
         fflush(stderr);
         return(-1);
      }

   }

   return(totalBytes);
}

/****************************************************************************/

int countdirentries(char *dirname)
{
   struct dirent *holdent, *holdsub;
   DIR *thisdir, *subdir;
   FILE *curfile;
   int i, retcode;
   char subdirname[100], fbuff[100];

   i = 0;

   if ((thisdir = opendir(dirname)) == NULL)  {
      if (major_count < 20)  {
         major_count++;
         perror("Open directory ");
         fprintf(stderr, "\nCan't count files!!\n");
         fprintf(stderr, "Will not accept any files till corrected\n");
         fflush(stderr);
      }
      return(-1);
   }


   rewinddir(thisdir);
   while ((holdent = readdir(thisdir)) != NULL)  {

      if (holdent->d_name[0] != '.')  {
         strcpy(subdirname, dirname);
         strcat(subdirname, "/");
         strcat(subdirname, holdent->d_name);
         if ((subdir = opendir(subdirname)) == NULL)  {
            if (major_count < 20)  {
               major_count++;
               perror("Open sub directory ");
               fprintf(stderr, "\nCan't count files!!\n");
               fprintf(stderr, "Will not accept any files till corrected\n");
               fflush(stderr);
            }
            closedir(thisdir);
            return(-1);
         }
         rewinddir(subdir);
         while ((holdsub = readdir(subdir)) != NULL)   {

            if (holdsub->d_name[0] != '.')  {
               i++;
            }
         }
         closedir(subdir);

         major_count = 0;

      }
   }

   closedir(thisdir);
   return(i);
}


/**************************************************************************/


void getvalidconn(void)
{
   FILE *fptr;
   int i, foundme=0;
   char filename[100], mbuff[81], holdname[81];

   i = gethostname(myname, 80);

   strcpy(filename, QPath);
   strcat(filename, "/tcpconn.dat");

   if ((fptr = fopen(filename, "r")) == NULL)  {
      perror("Can't open tcpconn table ");
      exit(1);
   }

   do {
      if (!feof(fptr)) {
         fgets(mbuff, 80, fptr);
         if (strncmp(mbuff, myname, strlen(myname)) != 0)
            continue;
         else  {
            foundme=1;
            break;
         }
      }
      else  {
         break;
      }
   } while (1);

   if (foundme)  {
      for (i=0; i<10; i++)
         clientname[i]=mbuff[i+10];
      clientname[i]=0;
   }
   else  {
      fprintf(stderr, "Can't find me in tcpconn table!\n");
      exit(1);
   }

   for (i=0; clientname[i]!=' '; i++);
   clientname[i]=0;

   fprintf(stderr, "Client allowed is '%s'\n", clientname);
   fclose(fptr);
   return;
}


