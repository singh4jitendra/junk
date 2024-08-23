/**********************************************************************
 *
 * totcp.c - This program will be used to talk to the Tandem via 
 *         TCP/IP and get the files in the twhs directory to pass
 *         to the Tandem.
 *
 * Compiling instructions ...
 *   cc totcp.c -ototcp -lsocket -lnsl -lresolv -lx
 *
 * J. Shaver - G. T. Systems - 01/07/97
 *
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/signal.h>
#include <sys/fcntl.h>

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
#include <time.h>
#include <stdlib.h>

#define INET_ERROR 4294967295  /* inet addr return 0xffffffff1 upon error */
#define BEGIN_TRANS "$$QSTX"
#define END_TRANS   26         /* 1A - Eof marker */
#define RESET_TRANS 04         /* 04 - Eot marker */

/*********************** Global Variables *******************************/

struct hostent *hp;
struct servent *sp;

struct sockaddr_in myaddr_in, peeraddr_in;

struct tm *curtime;
time_t bintime;
char buff[513], trancode[4], seqname[100];
char servname[30], begin_trans[10], end_trans[3];
char whsdir[256];
DIR *newdir;
struct dirent *newent;
int ls;
DIR *datafiles[1000];
int maxcounts=0;
char tran_names[1000][50], countdata[1000][81];
char errfile[100];


/************************ Function Prototypes ****************************/

char *getenv();
void checkparams(int argc, char *argv[]);
int totcp(void);
int catchsig(int);
int countdirentries(void);
int processfiles(void);
int setuptrans(int);
int sendtrans(char *, char *);
void closealldir(void);


/*********************** Main Program *************************************/

main(argc, argv)
int argc;
char *argv[];
{

int i, retcode;

/* Declare function to be associated with the signals to be caught */
int catchsig();

checkparams(argc, argv);

/* Which signals are to be caught */

signal(SIGINT, catchsig);
signal(SIGUSR1, catchsig);

maxcounts=0;
for (i=0; i<1000; i++)  {
   datafiles[i]=0;
   tran_names[i][0]=0;
   countdata[i][0]=0;
}

memset((char *)&myaddr_in,0,sizeof(struct sockaddr_in));
memset((char *)&peeraddr_in,0,sizeof(struct sockaddr_in));

/* Attempt to get the IP address of who to connect to */
myaddr_in.sin_family = AF_INET;
if ((myaddr_in.sin_addr.s_addr = inet_addr(argv[1])) == INET_ERROR)  {
   if ((hp = gethostbyname(argv[1])) == NULL)  {
      printf("Get host by name failed - error %d\n", errno);
      exit(1);
   }
   myaddr_in.sin_addr.s_addr=*(u_int32_t *)(*(hp->h_addr_list));
}

/* Set the port number */
if ((sp = getservbyname(servname, "tcp")) == NULL)  {
   printf("CLIENT : requested service not on file!\n");
   exit(1);
}
myaddr_in.sin_port = sp->s_port;

/* Let's open the socket */
if ((ls = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
   perror("CLIENT: socket open failure ");
   exit(1);
}


while (1)  {
   retcode = totcp();
   if (retcode)  {
      if ((ls = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
         perror("CLIENT: socket open failure ");
         exit(1);
      }
   }
   sleep(4);
}

}  /* end Main */

/**************************************************************************/

int totcp()
{

int retcode, notok, i;
int addrLen, retCode, notconnected=1;
static int errcnt=0;

while (1)  {
   /* see if any files are available to go over */
   notok = countdirentries();
   if (!notok)  {
      if (!notconnected)  {
         close(ls);
         return(1);
      }
      return(0);
   }

   if (notconnected)  {
      while (1)  {
         if (connect(ls,(struct sockaddr *)&myaddr_in,(int)(sizeof(struct sockaddr_in))) < 0)  {
            if (errcnt < 10)  {
               errcnt++;

               time(&bintime);
               curtime = localtime(&bintime);
               perror("CLIENT: Can't connect ");
               fprintf(stderr, "\nPlease check the Tandem server!\n");
               fprintf(stderr, "Error at %s\n", asctime(curtime));
               fflush(stderr);
            }
            close(ls);
            if ((ls = socket(AF_INET, SOCK_STREAM, 0)) < 0)  {
               perror("CLIENT: socket open failed ");
               exit(1);
            }
         }
         else
            break;
      }
  
      errcnt = 0;
      notconnected = 0;
   }

   retcode = processfiles();
   if (retcode)  {
      close(ls);
      closealldir();
      return(1);
   }
}

}


/*****************************************************************************/

int processfiles(void)
{
   
   int datatoproc=1, i, retcode;

   while (datatoproc)  {
      for (i=0; i<maxcounts; i++)  {
         if (datafiles[i] != 0)  {
            retcode = setuptrans(i);
            if (retcode)
               return(1);
         }  
      }

      datatoproc=0;
      for (i=0; i<maxcounts; i++)  {
         if (datafiles[i] != 0)
            datatoproc=1;
      }
   }

   return(0);
}

/******************************************************************************/

int setuptrans(int filenum)
{
   int bytes, count=0, retcode, i;
   char buff[81], filename[100], fullname[100];

   /* Process up to 10 files in each subdirectory */
   while (count < 10)  {
      strcpy(errfile, whsdir);
      strcat(errfile, "/");
      strcat(errfile, countdata[filenum]);
      strcat(errfile, ".err");

      strcpy(fullname, whsdir);
      strcat(fullname, "/");
      strcat(fullname, tran_names[filenum]);
      strcat(fullname, "/");
      strcat(fullname, countdata[filenum]); 

      /******JLS change to bypass .tmp files ******/
      if ((strstr(fullname, ".tmp")) == NULL)  {
         retcode = sendtrans(fullname, tran_names[filenum]);
         if (retcode)
            return(1);
      }

      count++;

      while ((newent = readdir(datafiles[filenum])) != NULL)  {
         if (newent->d_name[0] != '.')  {
            strcpy(countdata[filenum], newent->d_name);
            break;
         }
      }

      if (newent == NULL)  {
         closedir(datafiles[filenum]);
         datafiles[filenum]=0;
         tran_names[filenum][0]=0;
         countdata[filenum][0]=0;
         return(0);
      }
   }
   
   return(0);
}

/*************************************************************************/

int sendtrans(char *filename, char *tranname)
{
   int bytes, filenum, retbytes;
   int numtries, retcode, i;
   char sndbuff[100];
   struct flock fl;

   numtries=0;

   strcpy(sndbuff, begin_trans);
   strcat(sndbuff, tranname);
   strcat(sndbuff, "   ");

   time(&bintime);
   curtime = localtime(&bintime);


   if ((filenum = open(filename, O_RDWR)) < 0)  {
      perror("CLIENT: Open trans ");
      fprintf(stderr, "Can't open '%s' at %s\n", filename, asctime(curtime));
      fflush(stderr);
      return(1);
   }

   /* The locking function is not available in Linux, the fcntl function
     must be used instead.  Project 04041.
   */
   fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = (off_t)0;
	fl.l_len = (off_t)0;
   retcode = fcntl(filenum, F_SETLK, &fl);
   if (retcode < 0)  {
      close(filenum);
      return(0);
   }

   bytes = read(filenum, buff, 512);

   if (bytes != 0)  {
      for (i=10; i<13; i++)  {
         if (!isdigit(buff[i])) {
            fprintf(stderr, "Bad tran code in %s\n", filename);
            fflush(stderr);
            close(filenum);
            rename(filename, errfile);
            return(0);
         }
      }

      if ((strcmp(tranname, "900") == 0))  {
         strcpy(sndbuff, begin_trans);
         strncat(sndbuff, &buff[10], 3);
         strcat(sndbuff, "   ");
      }
      else  {
         if ((strcmp(tranname, "470") == 0))  {
            strcpy(sndbuff, begin_trans);
            strncat(sndbuff, buff, 3);
            strcat(sndbuff, "   ");
         }
      }

      retbytes = SendData(ls, sndbuff, 12, 0);
      if (retbytes == 0 || retbytes == -1)  {
         fprintf(stderr, "Problem sending header for: %s\n",filename);
         fprintf(stderr, "  Closing file and connection\n");
         fflush(stderr);  
         close(filenum);
         return(1);
      }

      numtries=0;

      while (bytes > 0)  {
         retbytes = SendData(ls, buff, bytes, 0);
         if (retbytes == 0 || retbytes == -1)  {
            fprintf(stderr, "Couldn't send data for: %s\n", filename);
            fprintf(stderr, "   will try later\n");
            fflush(stderr);
            close(filenum);
            return(1);
         }
         bytes = read(filenum, buff, 512);
      }

      retbytes = SendData(ls, end_trans, 1, 0);
      if (retbytes != 1)  {
         perror("CLIENT: Can't send END ");
         fprintf(stderr, "\nClosing file and connection\n");
         fflush(stderr);
         close(filenum);
         return(1);
      }

      retbytes = recv(ls, buff, 2, 0);
      if (retbytes != 2)  {
         perror("Can't recv 2 bytes ");
         fprintf(stderr, "\nClosing file and connection\n");
         fflush(stderr);
         close(filenum);
         return(1);
      }
      if (buff[0] != 'O' || buff[1] != 'K')  {
         fprintf(stderr, "OK not sent\nError file is:%s\n",errfile);
         fflush(stderr);
         close(filenum);
         rename(filename, errfile);
         return(0);
      }
   }
   close(filenum);
   unlink(filename);
   return(0);
}

/**************************************************************************/
/* Function to handle SIGINT and SIGUSR1                                  */
/**************************************************************************/

catchsig(signo)
int signo;
{

   closealldir();
   close(ls);
   fprintf(stderr,"\nCATCHSIG: signo=%d\n",signo);
   exit(0);
}

/***************************************************************************/

void checkparams(argc, argv)
int argc;
char *argv[];
{
   char *tempdir;

   if (argc < 3 || argc > 4)  {
      printf("Usage is: %s hostname servname whsdir\n", argv[0]);
      printf("      Where hostname is the TCP host to connect to\n");
      printf("      servname is the name in the services file\n");
      printf("      whsdir is the directory where the files are located\n");
      exit(1);
   }

   strcpy(servname, argv[2]);

   if (argc == 4)
      strcpy(whsdir, argv[3]);
   else
      if ((tempdir = getenv("QTRANS")) == NULL)  {
         perror ("Error - No whs directory");
         exit(1);
      }
      else  {
         strcpy(whsdir, tempdir);
         strcat(whsdir, "/twhs");
      }

   if (whsdir[strlen(whsdir)-1]== '/') whsdir[strlen(whsdir)-1]='\0';

   strcpy(begin_trans, BEGIN_TRANS);
   end_trans[0] = END_TRANS;
   end_trans[1] = 0;

   return;
}

/************************************************************************/

int SendData(int s, char *buffer, int bufSize, int flags)
{
   int totalBytes = 0, bytesSent = 0;

   totalBytes = send(s, buffer, bufSize, flags);
   if ((totalBytes == -1) || (totalBytes == 0)) {
      if (totalBytes == -1)
         perror("SEND failed ");
      return(totalBytes);
   }

   while (totalBytes < bufSize)  {
      bytesSent = send(s, buffer+totalBytes, bufSize-totalBytes,
                       flags);
      if (bytesSent == -1 || bytesSent == 0)  {
         if (bytesSent == -1)
            perror("SEND failed ");
         return(bytesSent);
      }
      totalBytes += bytesSent;
   }

   return(totalBytes);
}

/***************************************************************************
  Return true if there are files in the twhs directory to be sent over
  and fill up the tran_names array with the subdir names and
  set the datafiles array to the dir pointer for them.
***************************************************************************/
int countdirentries()
{

   struct dirent *holdent;
   DIR *thisdir;
   FILE *curfile;
   int i=0, retcode=0, retsys, closethis=0;
   char dirname[100];

   if ((thisdir = opendir(whsdir)) == NULL)  {
      perror("CLIENT: Open directory ");
      exit(1);
   }

   rewinddir(thisdir);
   maxcounts=0;

   while ((holdent = readdir(thisdir)) != NULL)  {
      if (holdent->d_name[0] != '.')  {
         strcpy(dirname, whsdir);
         strcat(dirname, "/");
         strcat(dirname, holdent->d_name);
   
         closethis=1;

         if ((newdir = opendir(dirname)) != NULL)  {
            rewinddir(newdir);
            while ((newent = readdir(newdir)) != NULL)  {
               if (newent->d_name[0] != '.')  {
                  strcpy(tran_names[maxcounts], holdent->d_name);
                  datafiles[maxcounts]=newdir;
                  strcpy(countdata[maxcounts], newent->d_name);
                  maxcounts++;
                  retcode=1;
                  closethis=0;
                  break;
               }
            }
            if (closethis)
               closedir(newdir);
         }  /* end if can open this dir (or is it a file?)  */
      }  /* end if not a hidden file */
   }  /* end while dir entries */
   
   closedir(thisdir);
   return(retcode);
}

/**************************************************************************/

void closealldir(void)
{

   int i;

   for (i=0; i<maxcounts; i++)  {
      if (datafiles[i] != 0)  
         closedir(datafiles[i]);
      datafiles[i]=0;
      tran_names[i][0]=0;
      countdata[i][0]=0;
   }

   maxcounts=0;
}


