/****  putq.c - This function is used to write data from an application
                to a flat file to be used in the WDC queuing system.


Compiling instructions ....
	For the 650:				For the 825:
		cc.10 putq.c -c			     cc putq.c -c
		copy putq.o to /WESCO/obj	     copy putq.o to 
						       /usr/acct/caroline/relsrc

    Author - R. Wood GT Systems.

    Date   - 06/12/91
*/

/*   July 24,1992  -  Added code to create subdirectories underneath
                      the $QTRANS directory. This was necessary because
                      the UNIX OS only allows 2000 files in a directory.
                      I created a counter, and for every 2000 files I
                      create I will create a new subdirectory.

   Changed: 12/22/04 mjb - Project 04041. Added a preconditional statement
                           to prevent sys_errlist from being declared
                           if _GNU_SOURCE is defined.
            10/03/05 mjb - Project 05264. Commented out code that wrote
                           to the /tmp/crafile debug file.
*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#define TOOBIG 56
#define FSTRING "%s,%s,%24.24s,%s\n"
#define FNAME   "/FILECNT"
#define D1999   "/1999"
#define D3999   "/3999"
#define D5999   "/5999"
#define D7999   "/7999"
#define D9999   "/9999"

/****  The following structure defines the data that is passed between
       the application and this function.   */

struct passdata {
   char command;
   char trantype[8];
   char inbuff[512];
   char qname[8];
   char recno[10];
   char retcode[2];
};

/****  Because of the nature of this function, most of its variables must
       be static and are therefore declared outside of the function.  */

struct passdata *indata;
FILE *seqfile,*outfile, *savfile;

#ifdef USE_CRAFILE
FILE *crafile;
#endif

int retval, seqlock, errcode, opflg=0;

int putq(),putqinit(),newfile(),writefle(),closenque();
int doaudit();

char user[16], savqname[16], savoname[16];
char seqname[100],outname[100],audname[100];
char *getenv(), *qtname, *qdname;

#ifndef _GNU_SOURCE
extern char *sys_errlist[];
#endif

/*----------------------------------------------------------------*/
int PUTQ(in)
struct passdata *in;
{
if((qdname = getenv("QDATA")) == NULL) {
   printf ("ERROR: Directory environment params not set\n");
   strcpy(indata->retcode,"9");
   return(0);
}
if((qtname = getenv("QTRANS")) == NULL) { 
   printf ("ERROR: Directory environment params not set\n");
   strcpy(indata->retcode,"9");
   return(0);
}

indata = in;
putqinit();
strcpy (savqname, indata->qname);

/****  Let's determine what the application wants us to do.  */

switch(indata->command) {

		/* Inquire if queue manager is up */

   case 'I':	retval = qmgrup();
		sprintf(indata->retcode,"%d",retval);
		break;

		/* Open a new transaction file */

   case 'O':	retval = qmgrup();
		sprintf(indata->retcode,"%d",retval);
		if (retval != 0) break;
		newfile();
		break;

		/* Write data to an already open file */

   case 'W':	writefle();
		break;

		/* Close the current file and queue it */

   case 'C':	closenque();
		break;

		/* Reset - Close and delete current file */

   case 'R':	resetque();
		break;

   default:	strcpy(indata->retcode,"9");
}
return(0);
}

/*----------------------------------------------------------------*/
/****  The following function is used to open a new file.  The filename
       is built by using the transaction type and a sequence number.
       Whatever is passed in 'inbuff' will be logged to an audit file
       along with the new file name, and a date/time stamp.  */

int newfile()
{
char tbuff[100];
char innum[5];
char fnum[5];
int snum,snum2,i;

#ifdef USE_CRAFILE
crafile = (FILE *)fopen("/tmp/crafile","a+");
fprintf (crafile,"NEW RUN");
fclose (crafile);
#endif

/**** Create the name of the sequence file that we will be using
      to create the subdirectories. Open and lock it so other 
      users have to wait to update the number. This file will be used to
      create the name of the transaction file. */

strcpy (tbuff,qdname);
strcat (tbuff,FNAME);

if ((seqlock = open(tbuff,O_RDWR)) < 0) {
   strcpy(indata->retcode,"3");
   return(0);
}
seqfile = (FILE *)fdopen(seqlock,"r+");
lockf(seqlock,1,0);

/**** Read in the current file number */

if (fread(fnum,4,1,seqfile) < 0) {
   strcpy(indata->retcode,"4");
   return(0);
}

fnum[4] = 0;
snum2 = atoi(fnum);

/**** Increment the file number and rewrite.  Close file.*/

if (snum2 == 9999) snum2 = 1;
else snum2++;
rewind(seqfile);
fprintf(seqfile,"%04d",snum2);
rewind(seqfile);
lockf(seqlock,0,0);
fclose(seqfile);

/**** Create the name of the sequence file, open and lock it so other 
      users have to wait to update the number. This file will be used to
      create the name of the transaction file. */

sprintf(seqname,"%s/%sSEQ",qdname,indata->trantype);
if ((seqlock = open(seqname,O_RDWR)) < 0) {
   strcpy(indata->retcode,"3");
   return(0);
}
seqfile = (FILE *)fdopen(seqlock,"r+");
lockf(seqlock,1,0);

/**** Read in the current number to use for a transaction file name. */

if (fread(innum,4,1,seqfile) < 0) {
   strcpy(indata->retcode,"4");
   return(0);
}

/**** Create the name of the transaction file. */

innum[4] = 0;
snum = atoi(innum);
sprintf(savoname,"%s%04d",indata->trantype,snum);

#ifdef USE_CRAFILE
crafile = (FILE *)fopen("/tmp/crafile","a+");
#endif

strcpy (tbuff,qtname);

#ifdef USE_CRAFILE
fprintf(crafile,"tbuff 1 = %s     snum2 = %d\n",tbuff,snum2);
#endif

if (snum2 > 0    && snum2 < 2000)  strcat (tbuff,D1999);
if (snum2 > 1999 && snum2 < 4000)  strcat (tbuff,D3999);
if (snum2 > 3999 && snum2 < 6000)  strcat (tbuff,D5999);
if (snum2 > 5999 && snum2 < 8000)  strcat (tbuff,D7999);
if (snum2 > 7999 && snum2 < 10000) strcat (tbuff,D9999);

#ifdef USE_CRAFILE
fprintf(crafile,"tbuff 2 = %s\n",tbuff);
fclose (crafile);
#endif

sprintf(outname,"%s/%s%04d",tbuff,indata->trantype,snum);

/**** Increment the sequence number and rewrite to sequence file.
      Close file.*/

if (snum == 9999) snum = 1;
else snum++;
rewind(seqfile);
fprintf(seqfile,"%04d",snum);
rewind(seqfile);
lockf(seqlock,0,0);
fclose(seqfile);

/**** Write a record to the audit file. */

doaudit(0);

/**** Open transaction file. */

if ((outfile = (FILE *)fopen(outname,"w")) == NULL) {
   strcpy(indata->retcode,"3");
   return(0);
}
opflg = 1;
strcpy(indata->retcode,"0");
return(0);
}
/*----------------------------------------------------------------*/
/****  The following function is used to write a record to the audit
       file. */

int doaudit(delswch)
int delswch;
{
FILE *audfile;
long intime;
struct tm *mytime;
if ((audfile = (FILE *)fopen(audname,"a")) == NULL) {
   strcpy(indata->retcode,"3");
   return(0);
}
intime = time((long *) 0);
mytime = localtime(&intime);
if (delswch == 0) {
    if((cuserid(NULL)) == NULL) {
          fprintf(audfile,FSTRING,
          savoname,indata->inbuff,asctime((const struct tm *)mytime),"XXXXX");
    }
    else  fprintf(audfile,FSTRING,
          savoname,indata->inbuff,asctime((const struct tm *)mytime),cuserid(NULL));
}
else fprintf (audfile,FSTRING,
	  savoname,"COMPLETED",asctime((const struct tm *)mytime),savqname);
fclose(audfile);
}

/*----------------------------------------------------------------*/
/****  The following routine is used to write a record to the current
       file.  */

int writefle()
{
if (opflg == 0) {        /*  verify we have a file opened.  */
   strcpy(indata->retcode,"6");
   return(0);
}
fprintf(outfile,"%s\n",indata->inbuff);
strcpy(indata->retcode,"0");
}

/*----------------------------------------------------------------*/
/****  The following routine is used to close the currently opened file
       and queue it's name to the WDC queues.  */


int closenque()
{
int retry=0;
char dirname[100];
char chkuser[16], chksavq[16], chkdir[100];
char buffit[512];

FILE *savfile;

fclose(outfile);

doaudit(1);

/* Attach path to filename */


strcpy(dirname,outname);
 
if((cuserid(NULL)) == NULL) {
   strcpy(user,"XXXXX");
}
else strcpy(user,cuserid(NULL));
retry_it:
retval = queueit(dirname,savqname);
if (retval == 8 && retry < 3) {
	retry++;
	goto retry_it;
}
sprintf(indata->retcode,"%d",retval);

/**** The below "if statement" is for when the Queue Manager 
      goes down between the call of "writefle" and "closenque".
      We will write out the necessary info and when the Queue
      Manager comes back these files will be queued. */

if (retval != 0) {

	/**** Open temporary save file. */

	strcpy(buffit,qdname);
	strcat(buffit,"/");
	strcat(buffit,"savfile");
	if ((savfile = (FILE *)fopen(buffit,"a+")) == NULL) {
	   strcpy(indata->retcode,"3");
	   return(0);
	}
	fprintf(savfile,"%s %s %s\n",user,savqname,dirname);
	fclose (savfile);
}
return(0);
}
/*----------------------------------------------------------------*/
/****  The following function is used to setup the data for easy
       manipulations  ( put nulls at the end of strings, etc)  */

int putqinit()
{
int i;
for (i=0; indata->trantype[i] != ' ' && i < 7;i++);
indata->trantype[i] = 0;

for (i=510; i>=0 && indata->inbuff[i] == ' ';i--);
indata->inbuff[i+1] = 0;

for (i=0; indata->qname[i] != ' ' && i < 7;i++);
indata->qname[i] = 0;

strcpy(audname,qdname);
strcat(audname,"/");
strcat(audname,"audit.trl");
return(0);
}
#include "qmgrup.c"
#include "queueit.c"

/*----------------------------------------------------------------*/
/*** This routine will close and delete the current transaction
     file being used */

resetque()
{
if (opflg == 1) {
   fclose (outfile);
}
unlink(outname);
return(0);
}
