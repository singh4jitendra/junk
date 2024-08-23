#ident "@(#)  updtspwd.c  Version  1.2  Date Retreived  1/14/92"
#include <stdio.h>
#include <sys/signal.h>
#include <termio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
extern int errno;
FILE *pwdfile,*nfile;

main(argc,argv)
int argc;
char *argv[];
{
char sflag[2];
char *epasswd;
char ststr[50];
char name[10];
char *crypt();
long readpwd();
struct tm *currday;
long currtime;
char dtstamp[10];
char pwd[20],lcode[10],lacc[10],lchg[10],att[5];
int i,j,cntr;
char buff[100];
int bufflen;

if (argc < 2) exit(1);
epasswd=crypt(argv[1],"W2");

currtime = time((long *) 0);
currday = gmtime(&currtime);
currday->tm_year %= 100;    /* insure 2 digit date, not 3 digit */
sprintf(dtstamp,"%02d%03d",currday->tm_year,currday->tm_yday);
if ((pwdfile=(FILE *)fopen("/usr/security/pwdfile","r")) == NULL) {
  perror("fopen");
  exit(1);
}
if ((nfile=(FILE *)fopen("/tmp/npwdfile","w")) == NULL) {
   perror("fopen of newfile");
   exit(1);
}
while (fgets(buff,80,pwdfile) != NULL) {
   i = 0;
   bufflen = strlen(buff);
   while ((buff[i] != ':') && (i < bufflen)) {
      name[i] = buff[i];
      i++;
   }
   name[i] = 0;
   pwd[0] = ststr[0] = lcode[0] = lacc[0] = lchg[0] = att[0] = sflag[0] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      pwd[j] = buff[i];
      i++;
      j++;
   }
   pwd[j] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      ststr[j] = buff[i];
      i++;
      j++;
   }
   ststr[j] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      lcode[j] = buff[i];
      i++;
      j++;
   }
   lcode[j] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      lacc[j] = buff[i];
      i++;
      j++;
   }
   lacc[j] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      lchg[j] = buff[i];
      i++;
      j++;
   }
   lchg[j] = 0;
   i++;
   j=0;
   while ((buff[i] != ':') && (i < bufflen)) {
      att[j] = buff[i];
      i++;
      j++;
   }
   att[j] = 0;
   i++;
   if (argc < 3) {
     if (buff[i] == 'Y')  {
        strcpy(sflag,"Y");
        strcpy(pwd,epasswd);
        strcpy(lacc,dtstamp);
        strcpy(lchg,dtstamp);
     }
     else sflag[0] = 0;
   }
   else {
      for (cntr=2; cntr < argc; cntr++)
        if (!strcmp(argv[cntr],name)) {
            strcpy(pwd,epasswd);
            strcpy(lacc,dtstamp);
            strcpy(lchg,dtstamp);
            break;
         }
    }   
    strcpy(sflag,(buff[i] == 'Y')?"Y":"");
   fprintf(nfile,"%s:%s:%s:%s:%s:%s:%s:%s\n",name,pwd,ststr,lcode,lacc,lchg,att,sflag);
}
fclose(nfile);
fclose(pwdfile);
exit(0);
}
