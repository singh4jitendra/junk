#ident "@(#)Module: cvtpasswd.c  Version 1.5  Last Modified 10/23/90"
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <time.h>
#include <stdlib.h>

char *SCCSID = "@(#)Module: cvtpasswd.c  Version 1.5  Last Modified 10/23/90";
char dtstamp[10];
char *epwd; 
char sbuff[200][100],wbuff[200][100];

main(argc,argv)
int argc;
char *argv[];
{
int i,j;
long currtime;
struct tm *currday;
char *crypt();
FILE *passwd,*pwdfile;
char buff[80];
int sidx,widx,bufflen;
char name[16],pwd[20],uid[5],gid[5],msg[50],dir[50],startstr[50];
char pgm[80];

strcpy(pgm,"/usr/security/secure");
currtime = time((long *) 0);
currday = gmtime(&currtime);
sprintf(dtstamp,"%02d%03d",currday->tm_year%100,currday->tm_yday);
epwd = crypt("wescom2","W2");

if ((passwd = (FILE *)fopen("/etc/passwd","r")) == NULL) {
   perror("Error opening passwd");
   exit(1);
}
sidx = widx = 0;
while(fgets(buff,80,passwd) != NULL) {
   i = 0;
   j = 0;
   bufflen=strlen(buff);
   if (buff[bufflen-1] == 10) bufflen--;
   while((buff[i] != ':') && (i < bufflen)) {
     name[j] = buff[i];
     i++;
     j++;
   }
   name[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     pwd[j] = buff[i];
     i++;
     j++;
   }
   pwd[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     uid[j] = buff[i];
     i++;
     j++;
   }
   uid[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     gid[j] = buff[i];
     i++;
     j++;
   }
   gid[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     msg[j] = buff[i];
     i++;
     j++;
   }
   msg[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     dir[j] = buff[i];
     i++;
     j++;
   }
   dir[j] = 0;
   i++;
   j = 0;
   while((buff[i] != ':') && (i < bufflen)) {
     startstr[j] = buff[i];
     i++;
     j++;
   }
   startstr[j] = 0;
   if (strcmp("0",uid) == 0) {
     sprintf(sbuff[sidx],"%s::%s:%s:%s:%s:%s\n",name,uid,gid,msg,dir,pgm);
     sprintf(wbuff[widx],"%s:%s:%s: :%s:00000:0:Y\n",name,epwd,startstr,dtstamp);
     sidx++;
     widx++;
   }
   else if (strcmp("101",gid) == 0) {
     sprintf(sbuff[sidx],"%s::%s:%s:%s:%s:%s\n",name,uid,gid,msg,dir,pgm);
     sprintf(wbuff[widx],"%s:%s:%s: :%s:00000:0:\n",name,epwd,startstr,dtstamp);
     sidx++;
     widx++;
   }
   else {
     strcpy(sbuff[sidx],buff);
     sidx++;
   }
}
fclose(passwd);
if ((passwd = (FILE *)fopen("/tmp/passwd.new","w")) == NULL) {
    perror("opening tmp/passwd.new");
    exit(1);
}
if ((pwdfile = (FILE *)fopen("/tmp/pwdfile.new","w")) == NULL) {
    perror("opening tmp/pwdfile.new");
    exit(1);
}
for(i=0;i<sidx;i++) fprintf(passwd,"%s",sbuff[i]);
for(i=0;i<widx;i++) fprintf(pwdfile,"%s",wbuff[i]);
fclose(pwdfile);
fclose(passwd);
exit(0);
}
