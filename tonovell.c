/*************************************************************************
*
*   tonovell.c   -  This program is used to copy transaction files from 
*                   the queueing system to a directory/subdirectory 
*                   scheme using the transaction name as the sudirectory
*                   name.  This program was developed as a replacement to
*                   the totandem.c program.
*
*   Date : 9/11/95
*
*   Author:  R. Wood - GT Systems
*
*
*   Special Instructions:
*
*             This program requires the getq.o module be linked into it.
*
**************************************************************************/
/* JLS - 10/24/95 - Fix countdirentries function.                        */
/* JLS - 11/29/95 - Put 470's and all 900 tran codes in one dir          */
/* JLS - 01/07/97 - When first create the file, put .tmp on end so the
                    other system will leave it alone till we're done!!
*/
/* JLS - 02/97    - Count number of trans before counting dir entries    */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/uio.h>
#include <dirent.h>

/****************************    End of Includes    **********************/

/****************************    Setup default values  ******************/

#define  SEQFNAME          "/IMOSDATA/2/QUEUES/QDATA/tonovell.seq"
#define  QNAME             "TOTAND"
#define  MAXDIRENTS        500 
#define  SLEEPAMOUNT       30

/****************************    End of Defaults  ************************/

/****************************    Setup Prototypes ***********************/

void checkusage(int , char **);
void loadenvironment(void);
int countdirentries(char *);
int gettrannumber(void);
void resettran(void);
void abort_prog(void);
void copyfile(char *, char *);
void callgetq(void);
int  catchsig(int);

/*****************************   End Prototypes  *************************/

/****************************   Global Variables  ***********************/

char seqfilename[128];
char queuename[12];
int tranfilenumber;
int numtrans=0;
char do_delete=0;
char newfname[256], tmpname[256];
int  infile, outfile;
FILE *fptr;


struct qstruct {
   char command;
   char trantype[8];
   char inbuff[512];
   char qname[8];
   char recno[10];
   char retcode[2];
} queuedata;

/*****************************  Start the program  ************************/

main(int argc, char *argv[]) 
{
int i;
char dirname[256];
char currdirname[256];
char trancode[10];
DIR *holddir;

/*   Start of Initialization   */

   checkusage(argc,argv);
   loadenvironment();
   signal(SIGINT, catchsig);
   signal(SIGUSR1, catchsig);

   strcpy(queuedata.qname,queuename);
   queuedata.command = 'S';
   strcpy(dirname,argv[1]);
   if (dirname[strlen(dirname)-1] == '/') dirname[strlen(dirname)-1] = 0;

/*    End of Initialization    */

   while(1) {

/*  Check that we aren't filling up the directories   */
      if (numtrans == 0)
         while(countdirentries(argv[1]) > MAXDIRENTS) sleep(SLEEPAMOUNT);

/*  Get a record from the queue .... Wait until there is one. */
      numtrans++;
      if (numtrans > 500)
         numtrans = 0;

      callgetq();

/*  Build the full directory name and create it if neccessary   */

      tranfilenumber = gettrannumber();
      sprintf(trancode,"%3.3s",queuedata.inbuff+strlen(queuedata.inbuff)-7);
      strcpy(currdirname,dirname);
      strcat(currdirname,"/");
      
      /* Some trans should stay together as a group  */
      if (strncmp(trancode, "47", 2)==0)
         strcat(currdirname, "470");
      else if (trancode[0] == '9')
         strcat(currdirname, "900");
      else
         strcat(currdirname,trancode);
      if ((holddir = opendir(currdirname)) == NULL) {
         if (mkdir(currdirname,0777) != 0) {
            perror("Could not create directory");
            fprintf(stderr,"Directory name - %s\n",currdirname);
            exit(1);
         }
      }
      else {
         closedir(holddir);
         chmod(currdirname,0777);
      }
      sprintf(newfname,"%s/%06d",currdirname,tranfilenumber);

/*  Copy the transaction file to it's new home. */

      copyfile(queuedata.inbuff,newfname);

/* delete from queue  */

      do_delete=0;
      queuedata.command = 'D';
      callgetq();

      queuedata.command = 'W';

   }

   printf("# of entries = %d\n",i);
   printf("tranfilenumber = %d\n",tranfilenumber);
   printf("queuename = %s\n",queuename);
   exit(0);
}

int countdirentries(char *dirname)
{
struct dirent *holdent;
DIR *thisdir;
FILE *curfile;
int i=0, retcode;
char ls_command[100], fbuff[100];

   if ((thisdir = opendir(dirname)) == NULL) {
      perror("Open directory");
      exit(1);
   }
   rewinddir(thisdir);
   while ((holdent = readdir(thisdir)) != NULL) {
      if (holdent->d_name[0] != '.') {   
          strcpy(ls_command, "ls -l ");
          strcat(ls_command, dirname);
          strcat(ls_command, "/");
          strcat(ls_command, holdent->d_name);
	  strcat(ls_command, " >/tmp/counts");
	  if ((retcode = system(ls_command)) == -1)  {
	      perror("System command failed");
	      exit(1);
          }

	  if ((curfile = fopen("/tmp/counts", "r")) == NULL)  {
	      perror("Error - Can't open temp file");
	      exit(1);
          }

	  /*  bypass Total line  */
	  fgets(fbuff, 80, curfile);
	  if (!feof(curfile))  fgets(fbuff, 80, curfile);

	  while (!feof(curfile))  {
	      if (fbuff[0] != 'd')  i++;
	      fgets(fbuff, 80, curfile);
          }
	  fclose(curfile);
	  unlink("/tmp/counts");
      }  /* end if not hidden file  */
   }  /*  end while dir entries  */
   closedir(thisdir);
   return i;
}

void checkusage(int argc, char *argv[])
{
   if (argc < 2 || argc > 3) {
      printf("Invalid number of arguments!\n\nCorrect Usage:\n");
      printf("     %s dirname [queuename]\n\nWhere:\n",argv[0]);
      printf("     dirname - The name of the directory to write transactions to.");
      printf("     queuename - The name of the queue to read from.\n");
      exit(1);
   }

   if(argc != 3) 
      strcpy(queuename,QNAME);
   else 
      strcpy(queuename,argv[2]);

   return;
}

void loadenvironment(void)
{
char *holdptr;

  if((holdptr = getenv("TOSEQFILE")) == NULL) 
      strcpy(seqfilename,SEQFNAME);
  else 
      strcpy(seqfilename,holdptr);

  if ((fptr = fopen(seqfilename,"r+")) == NULL) {
      perror("Error - Open Sequence file");
      exit(1);
  }

  return;
}

int gettrannumber()
{
int i;

   rewind(fptr);
   
   if (fscanf(fptr,"%d",&i) != 1) {
      perror("Error - Reading Sequence file");
      exit(1);
   }
   rewind(fptr);
   fprintf(fptr,"%d",i+1);
   rewind(fptr);
   return i;
}

void resettran()
{
int i;

   rewind(fptr);  
   
   if (fscanf(fptr,"%d", &i) != 1)  {
      perror("Error - Reading Sequence file");
      exit(1);
   }
   rewind(fptr);
   i--;
   if (i<=0)  i=1;
   fprintf(fptr,"%d", i);
   rewind(fptr);
}

void abort_prog()
{
   close(outfile);
   if (do_delete)  {
      unlink(newfname); 
      resettran();
   }
   exit(1);
}

catchsig(signo)
int signo;
{
   fprintf(stderr,"\nCATCHSIG: signo=%d\n", signo);

   close(outfile);
   if (do_delete)  {
       unlink(newfname); 
       resettran();
   }
   fclose(fptr);
   exit(1);
}

void copyfile(char infname[],char outfname[])
{
int numbytes;
char fbuff[512];

   if ((infile = open(infname,O_RDONLY)) == -1) {
      perror("Open transaction file");
      fprintf(stderr, "Bypass of file %s\n", infname);
      fflush(stderr);
      /*  If not there, bypass and go on - JLS  */
      return;
   }

   /* JLS - add .tmp to name */
   strcpy(tmpname, outfname);
   strcat(tmpname, ".tmp");

   if ((outfile = open(tmpname,O_WRONLY | O_CREAT | O_EXCL,00666)) == -1) {
      perror("Open new output file");
      exit(1);
   }
   do_delete=1;
   fchmod(outfile,0666);
   while ((numbytes = read(infile,fbuff,512)) == 512) 
      if (write(outfile,fbuff,512) == -1) {
         fprintf(stderr,"Error writing file %s\n",tmpname);
         perror("Write error");
         exit(1);
      }
   if (numbytes > 0) {
      if (write(outfile,fbuff,numbytes) == -1) {
         fprintf(stderr,"Error writing file %s\n",tmpname);
         perror("Write error");
         exit(1);
      }
   }
   else if (numbytes < 0) {
      fprintf(stderr,"Error reading file %s\n",infname);
      perror("Read error");
      exit(1);
   }
   close(infile);
   close(outfile);
   rename(tmpname, outfname);

   return;
}

void callgetq()
{
int i;
/*  Strip off the nulls for GETQ  */

      for (i=7;queuedata.qname[i] == 0 && i >= 0;i--)
         queuedata.qname[i] = ' ';
      for (i=7;queuedata.trantype[i] == 0 && i >= 0;i--)
         queuedata.trantype[i] = ' ';
      for (i=9;queuedata.recno[i] == 0 && i >= 0; i--)
         queuedata.recno[i] = ' ';

      GETQ (&queuedata);


      if ((strncmp(queuedata.retcode,"A",1)) == 0) abort_prog();
      if ((strncmp(queuedata.retcode,"0",1)) != 0) {
         if ((strncmp(queuedata.retcode,"1",1)) == 0 || 
             (strncmp(queuedata.retcode,"2",1)) == 0)
             fprintf(stderr,"ERROR: Queue Manager problem - code = %s\n",
                   queuedata.retcode);
         else
             fprintf(stderr,"ERROR: Bad return status on GETQ - code = %s\n",
                   queuedata.retcode);
         abort_prog();
      }

/*   Put the nulls back on from GETQ   */

      for (i=0;queuedata.qname[i] != ' ' && i < 8;i++);
      queuedata.qname[i] = 0;
      for (i=7;queuedata.trantype[i] != ' ' && i >= 0;i--)
      queuedata.trantype[i] = 0;
      for (i=9;queuedata.recno[i] != ' ' && i >= 0; i--)
      queuedata.recno[i] = 0;

}
