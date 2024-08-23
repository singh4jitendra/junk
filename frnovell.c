/****************************************************************
*
*  frnovell.c - This program is used to pick up the files in the
*            Ksi directory and pass them along to the queueing
*            system.  It was developed as a replacement to
*            frtandem.c.
*
* Compiling instructions ....
*       cc -O4 -Hansi -s -ofrnovell frnovell.c putq.o  
*
*   J. Shaver  - G.T. Systems - 10/11/95
*/

/* 02/01/97 - JLS  Let's just check if qmgr is up once before we start thru
                   the dir looking for files.

   11/15/01 - JLS  Let's check to make sure there isn't another copy of
                   us running!  Causing multiple problems with double
		   receipts/shipping!!

   08/17/05 - JLS  Linux not creating file permissions on the lock file
                   the same as Unix does so change the open to explicitly
                   name the permissions.
*/


#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
/*  #include <btioctl.h>  */ 
#include <sys/signal.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/***************** Global Variables ****************************/

struct passdata {    		/* This is the format of the structure   */
   char command;		/* that is sent between this routine and */
   char trantype[8];		/* the PUTQ routine                      */
   char inbuff[512];
   char qname[8];
   char recno[10];
   char retcode[2];
} quedata;

char buff[513], trancode[4], seqname[100];
char ksidir[256], quename[20], do_reset=0, inname[256];
int  infile;
FILE * ksifile;
DIR * holddir;
struct dirent *holdent;
char dirname[256], pmsg[256]; 

/***************** Function Prototypes *************************/

char *getenv(); 
void checkparams(int argc, char *argv[]);
int  copyfile(void);
void frnovell(void);
int  catchsig(int);
int  abortit(void);
int  testname(char *);

/**************** Main Program *********************************/

main(argc,argv)
int argc;
char *argv[];
{

/* Declare function to be associated with SIGINT */
int catchsig();

checkparams(argc, argv);

/* Before signal call, SIGINT will terminate process */

signal (SIGINT, catchsig); /* After signal call, control will be passed */
		           /* to catchsig when SIGINT received */
signal (SIGUSR1, catchsig);

/*  Build the directory name  */
strcpy(dirname, ksidir);

/*  Open the directory  */
if ((holddir = opendir(dirname)) == NULL )  {
   strcpy(pmsg, "Error: Can't open directory - ");
   strcat(pmsg, dirname);
   perror(pmsg);
   exit(1);
}

/*  Loop forever: Call frnovell to process everything currently in the
                  directory, then sleep for 1 second so the CPU has
                  time to swap you out and proceed with other tasks.
*/

while (1)  {
    frnovell();
    sleep(1); 
}

closedir(holddir);

}
/***********************************************************/
void frnovell()
{
long lret;
int i, j, filenum, sysret, numbytes, notok;


   /* Check to see if qmgr is up  */
   quedata.command = 'I';
   PUTQ(&quedata); 
   if ((strncmp(quedata.retcode, "A", 1)) == 0) {
       abortit();
   }
   if ((strncmp(quedata.retcode, "0", 1)) != 0)  {
      printf("Error - Qmgr problem - %c\n", quedata.retcode[0]);
      abortit();
   }


/*  Rewind to the beginning and start through again  */
    rewinddir(holddir);

    while ((holdent = readdir(holddir)) != NULL)  {
        /* If it's not a hidden file...  */
        if (holdent->d_name[0] != '.')  {
            /*  Make complete filename with path  */ 
            strcpy(inname, dirname);
            strcat(inname, "/");
	    strcat(inname, holdent->d_name);

            /*  See if there's a tmp extension on the file - if there
                is, don't move since the warehs package isn't done
                putting data into it  */
            notok = testname(holdent->d_name);
            if (!notok)  {

               /*  See if we have data in this file - if not, leave
                   for the warehs package to delete  */
               notok = copyfile();
               if (!notok)  { 

                  /*  Tell PUTQ to close output  */
                  quedata.command = 'C';
                  strcpy(quedata.qname, quename);
                  PUTQ (&quedata);
                  
                  if ((strncmp(quedata.retcode,"A",1)) == 0)  {
                    unlink(inname);
                    do_reset = 0;
                    abortit();
                  }
                  if ((strncmp(quedata.retcode, "6", 1)) == 0) abortit();
                  if ((strncmp(quedata.retcode, "0", 1)) != 0)  {
                     fprintf(stderr,"Error - Qmgr return - %c\n", quedata.retcode[0]);
                     fflush(stderr);
                     abortit();
                  }

 
                  /*  Delete ksi file  */
                  unlink(inname);
                  do_reset = 0;
              }   /*  end if !notok  */
            }  /* end if !notok  */
 
        }  /* end if not a hidden file  */
       
   }  /*  end while directory entries   */


} 

/*-------------------------------------------------------------------------*/
/*  JLS - this function is used on the Close command's return of an
          A to tell us to shut down.  queueit.c is the only routine that
          does pass an A from qmgr.c and that is only called on a close
          of the data file.
*/
 
abortit()
{

if (do_reset)  {
    quedata.command = 'R';
    PUTQ(&quedata); 
}                     
fflush(stdout);
fseek(stderr, 0, SEEK_END);
fprintf(stderr, "Aborting program thru abortit\n");
fflush(stderr);
close(infile);
closedir(holddir);
exit(0);
}

/*-------------------------------------------------------------------------*/

/* Function to handle SIGINT */
/*  JLS - this function is used when the user says ALL DOWN since first
          qmgr is shutdown first and then the others are sent a kill 16  */

catchsig(signo)
int signo;
{
printf ("\nCATCHSIG: signo=%d\n",signo);
if (do_reset)  {
    quedata.command = 'R';
    PUTQ(&quedata);
}
close(infile);
closedir(holddir);
exit(0);
}


/**************************************************************/
void checkparams(argc, argv)
int argc;
char *argv[];
{
    char *tempdir;
    char lockname[100];
    FILE *fptr;
    int  ifile, ilock;


    if (argc < 3 || argc > 4)  {
	printf("Improper start to %s\n", argv[0]);
	printf("Usage is %s trantype qname whsdir\n", argv[0]);
	printf("Where trantype is 140, 515, etc. to process\n");
        printf("  qname is the name FRMTAND, 515FTAN, etc\n");
        printf("  and whsdir is the path name where the\n");
        printf(" warehouse directory is located.\n");
        exit(1);
    }

    strcpy(trancode, argv[1]);
    strcpy(quename, argv[2]);

    if (argc == 4)  
        strcpy(ksidir, argv[3]);
    else
        if ((tempdir = getenv("QTRANS")) == NULL)  { 
            perror("Error - No whs directory");
            exit(1);
        }
        else  {
            strcpy(ksidir, tempdir);
            strcat(ksidir, "/fwhs");
        }

    if (ksidir[strlen(ksidir)-1]=='/') ksidir[strlen(ksidir)-1]='\0';
    if (quename[strlen(quename)-1]=='\n') quename[strlen(quename)-1]='\0';

    strcat(ksidir, "/");
    strcat(ksidir, trancode);

/* now let's try out locks......*/
    strcpy(lockname, argv[0]);
    strcat(lockname, ".lock");
    if ((ifile = open(lockname, O_RDWR|O_CREAT,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
       fprintf(stderr, "Problem getting lock file!\n");
       fprintf(stderr, "Program aborting\n");
       exit(1);
    }
    fptr = (FILE *) fdopen(ifile, "r+");
    if ((ilock = lockf(ifile, 2, 0)) < 0)  {
        fprintf(stderr, "Can't get lock ---\n");
        fprintf(stderr, "Program ending!!!\n");
        exit(1);
    }
       

    return;
}

/******************************************************************/
int  copyfile()
{
int numbytes;
char fbuff[512];

    if ((infile = open(inname, O_RDONLY | O_EXCL)) == -1)  {
       return(1);
    }

    close(infile);
    ksifile = fopen(inname, "r");
    if (ksifile == NULL)
        return(1);

    /*  See if it's an empty file!  */
    fbuff[0] = '\0';
    fgets(fbuff, 511, ksifile);
    if ((numbytes = strlen(fbuff)) <= 10)  {
        fclose(ksifile);
        return(1);
    }
    else  {
        fclose(ksifile);
        ksifile = fopen(inname, "r");
        if (ksifile == NULL)
            return(1);
    }

    do_reset = 1;
 
    /*  Tell putq to open output  */
    quedata.command = 'O';
    strcpy(quedata.inbuff, inname);
    strcpy(quedata.qname, quename);
    strcpy(quedata.trantype, trancode);
    PUTQ(&quedata);
    
    /*  All OK?  */
    if ((strncmp(quedata.retcode, "A", 1))==0)  {
        abortit();
    }
    if ((strncmp(quedata.retcode, "1", 1))==0 ||  
        (strncmp(quedata.retcode, "2", 1))==0)  {
        printf("Error - Qmgr problem - %c\n", quedata.retcode[0]);
        abortit();
    }
    if ((strncmp(quedata.retcode, "3", 1))==0 ||
        (strncmp(quedata.retcode, "4", 1))==0)  {
        printf("Error - Seq file problem - %c\n", quedata.retcode[0]);
        abortit();
    }


    fgets(fbuff,511,ksifile);
    while (!feof(ksifile))  {
        /* take newline out of string since PUTQ will put one in  */
        fbuff[strlen(fbuff)-1] = '\0';

        /* send the data to putq  */
        quedata.inbuff[0] = 0;
	strncpy(quedata.inbuff, fbuff, 511);
        strcpy(quedata.qname, quename);
	quedata.command = 'W';
	PUTQ(&quedata);
        if ((strncmp(quedata.retcode, "A", 1)) == 0)  {
            abortit();
        }
        if ((strncmp(quedata.retcode, "1", 1)) == 0 ||
            (strncmp(quedata.retcode, "2", 1)) == 0)  {
            printf("Error - Queue Manager problem %c\n", quedata.retcode[0]);
            abortit();
        }
        if ((strncmp(quedata.retcode, "6", 1)) == 0)
            abortit();
        if ((strncmp(quedata.retcode, "0", 1)) != 0)  {
            fprintf(stderr,"Error - Qmgr code - %c\n", quedata.retcode[0]);
            fflush(stderr);
            abortit();
        }
    
        fgets(fbuff,511,ksifile);
    }


    fclose(ksifile);
    return(0);
}
/***************************************************************************/
int testname(char *tempname)
{

    int i, j, fnddot = 0;

    j = strlen(tempname);

    for (i=0; i < j; i++)  {
        if (tempname[i] == '.')  {
           fnddot = 1;
           i++;
           goto exitout;
        }
    }

exitout:
    if (fnddot)  {
       if (strcmp(&tempname[i], "tmp") == 0)  {
           return(1);
       }
    }

    return(0);
}

