#ident "@(#)  secure.c   Version  1.7  Date Retrieved  12/13/91"
#include <stdio.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <termio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include "ws_pwfile.h"
#include "msdefs.h"
#define CHGCODE 9

#ifdef _GNU_SOURCE
#include <errno.h>
#else
extern int errno;
#endif

FILE *pwdfile;  long findusr();     

main(argc,argv,envp)
int argc;
char *argv[],*envp[];
{
int i,retry;
char sflag[2];
char passwd[30];
char opasswd[30],*epasswd;
char newpasswd[30];
char startstr[50];
char usrname[10];
char lastaccess[6],lastchg[6],attempts[2],holdlast[6];
char langcode[5],langstrg[30];
char *crypt();
char envshell[80];
long filepos,readpwd();
long lastchange,lastacc;
int noattmp;
struct tm *currday;
long currtime,ldtstamp;
char dtstamp[10];
int rstflag,chgflag;
int y2k;
void cleanup();

signal(SIGINT,SIG_IGN);
signal(SIGQUIT,SIG_IGN);
signal(SIGHUP,(void(*)(int))cleanup);
signal(SIGILL,(void(*)(int))cleanup);
signal(SIGTRAP,(void(*)(int))cleanup);
signal(SIGFPE,(void(*)(int))cleanup);
signal(SIGBUS,(void(*)(int))cleanup);
signal(SIGSEGV,(void(*)(int))cleanup);
signal(SIGSYS,(void(*)(int))cleanup);

currtime = time((long *) 0);
currday = gmtime(&currtime);
currday->tm_year %= 100;   /* ensure 2 digit year, not 3 digit */
/* if the 2 digit year is less than 50 add 100 to provide for Y2K */
sprintf(dtstamp,"%02d%03d",currday->tm_year,currday->tm_yday);
if (currday->tm_year < 50) currday->tm_year += 100;
ldtstamp=((long) currday->tm_year*365) + currday->tm_yday;         /*100632*/
for (i=0;i<30;i++) {
   passwd[i] = 0;
   opasswd[i] = 0;
   newpasswd[i] = 0;
}
retry = 0;
strcpy(usrname,getenv("LOGNAME"));
filepos=readpwd(usrname,opasswd,startstr,langcode,lastaccess,lastchg,attempts,sflag);
if (filepos == (long) -1) {
   dropline();
   exit(0);
}

rstflag = 0;
noattmp = atoi(attempts);
strcpy(holdlast,lastchg);                                          /*100632*/
holdlast[2]=0;                                                     /*100632*/
/* if the 2 digit year is less than 50 add 100 to provide for Y2K */
/*sprintf(dtstamp,"%02d%03d",currday->tm_year,currday->tm_yday);*/
if ((y2k = atol(holdlast)) < 50) y2k += 100;
lastchange = (y2k*365) + atol(lastchg+2);                          /*100632*/
strcpy(holdlast,lastaccess);                                       /*100632*/
holdlast[2]=0;                                                     /*100632*/
/* if the 2 digit year is less than 50 add 100 to provide for Y2K */
/*sprintf(dtstamp,"%02d%03d",currday->tm_year,currday->tm_yday);*/
if ((y2k = atol(holdlast)) < 50) y2k += 100;
lastacc = (y2k*365) + atol(lastaccess+2);                          /*100632*/
if ((lastacc+90 < ldtstamp) && (sflag[0] != 'Y')) {
   printf("\nUserid has been locked due to inactivity.\n");
   printf("Please see your local administrator.\n");
   dropline();
   exit(0);
}
if ((noattmp > 7) && (sflag[0] == 'Y')) {
   noattmp = 7;
   rstflag = 1;
}

try_again:
if (noattmp > 7) {
   printf("Too many log on attempts... This ID is locked.\n");
   printf("Please see your local administrator.\n");
   dropline();
   exit(0);
}
printf("Password:");
chgflag=getpwd(passwd);
printf("\n");
       
epasswd = crypt(passwd,opasswd);
if (strcmp(epasswd,opasswd) == 0) {
   if (((lastchange+90 < ldtstamp) || (chgflag == 1)) && sflag[0] != 'Y' ){
      strcpy(newpasswd,opasswd);
      pwdchange(newpasswd,chgflag);
      strcpy(lastchg,dtstamp);
      epasswd = crypt(newpasswd,"$1$W2$");
      strcpy(opasswd,epasswd);
   }
   signal(SIGINT,SIG_DFL);
   signal(SIGQUIT,SIG_DFL);
   signal(SIGALRM,SIG_DFL);
   strcpy(attempts,"0");
   updtfile(filepos,usrname,opasswd,startstr,langcode,dtstamp,lastchg,attempts,sflag);
   fclose(pwdfile);
   printf("\n");
   if ((langcode[0] != 0) && (langcode[0] != ' ')) {
      sprintf(langstrg,"LANGCODE=%s",langcode);
      putenv(langstrg);
   }
   if ((startstr[0] == 0) || ( strcmp(startstr,"/bin/ksh") == 0)) {
      putenv("SHELL=/bin/ksh");
      execl("/bin/ksh","-sh",0);
      perror("exec1");
   }
   else {
      strcpy(envshell,"SHELL=");
      strcat(envshell,"/bin/ksh");
      putenv(envshell);
      execl(startstr,startstr,0);
      execl("/bin/ksh","-sh",startstr,0);
      perror("exec");
   }
   exit(0);
}
if (retry > 2) {
   dropline();
   exit(0);
}
printf("\nInvalid password - Please try again\n");
retry++;
noattmp++;
sprintf(attempts,"%d",noattmp);
updtfile(filepos,usrname,opasswd,startstr,langcode,lastaccess,lastchg,attempts,sflag);
goto try_again;
}
 
getpwd(inpass)
char *inpass;
{
char inchr;
char eof_save;
char eol_save;
int i,errstat;
struct termio hstats;
void trapper();

i = 0;
errstat = ioctl(0,TCGETA,&hstats);
if (errstat == -1) {
    perror("GET");
    fclose(pwdfile);
    exit(0);
}
hstats.c_lflag = hstats.c_lflag ^ ECHO;
eol_save = hstats.c_cc[5];
hstats.c_cc[5] = CHGCODE;
ioctl(0,TCSETA,&hstats);
signal(SIGALRM,(void(*)(int))trapper);

top_getpwd:
alarm(15);
errno = 0;
inchr =  getchar();
if (errno == 4) {
   printf("\nToo much time!!!!!\n");
   sleep(2);
   hstats.c_lflag = hstats.c_lflag ^ ECHO;
   hstats.c_cc[5] = eol_save;
   hstats.c_cflag = 0;
   ioctl(0,TCSETA,&hstats);
   fclose(pwdfile);
   exit(0);
}

alarm(0);
if (inchr < 32) {
   switch(inchr) {
      case CHGCODE: 	hstats.c_lflag = hstats.c_lflag ^ ECHO;
   			hstats.c_cc[5] = eol_save;
                        ioctl(0,TCSETA,&hstats);
                        ioctl(0,TCFLSH,2);
                        return(1);
      case 13: case 10: inpass[i] = 0;
                        hstats.c_lflag = hstats.c_lflag ^ ECHO;
   			hstats.c_cc[5] = eol_save;
                        ioctl(0,TCSETA,&hstats);
                        ioctl(0,TCFLSH,2);
                        return(0);
      default:
               goto top_getpwd;
   }
}
if (i < 16) {
   inpass[i] = inchr;
   i++;
}
else printf("\007");
goto top_getpwd;
}

long readpwd(usr,pwd,startstr,lcode,lacc,lchg,att,sflag)
char *usr,*pwd,*startstr,*lacc,*lcode,*lchg,*att,*sflag;
{
char buff[150],inname[10],*tokptr;
long cfpos;
int i,j,bufflen;
if ((pwdfile=(FILE *)fopen("/usr/security/pwdfile","r+")) == NULL) {
  perror("fopen");
  exit(0);
}
cfpos=ftell(pwdfile);
while (fgets(buff,150,pwdfile) != NULL) {
   i = 0;
   bufflen = strlen(buff);
   while ((buff[i] != ':') && (i < bufflen)) {
      inname[i] = buff[i];
      i++;
   }
   inname[i] = 0;
   if (strcmp(inname,usr) == 0) goto chkpwd;
   cfpos=ftell(pwdfile);
}
puts("Userid/password not on file");
fclose(pwdfile);
cfpos = -1;
return(cfpos);

chkpwd:
fclose(pwdfile);
pwd[0] = startstr[0] = lcode[0] = lacc[0] = lchg[0] = att[0] = sflag[0] = 0;
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
   startstr[j] = buff[i];
   i++;
   j++;
}
startstr[j] = 0;
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
if (buff[i] == 'Y') strcpy(sflag,"Y");
else strcpy(sflag, " ");
return(cfpos);
}

void trapper()
{
return;
}

dropline()
{
#if 0
struct termio hstats;

sleep(2);
ioctl(0,TCGETA,&hstats);
hstats.c_cflag = 0;
ioctl(0,TCSETA,&hstats);
#endif
sleep(1);
fclose(pwdfile);
return(0);
}

updtfile(pos,usr,pwd,sstr,lcode,la,lc,att,sflag)
char *usr,*pwd,*sstr,*la,*lcode,*lc,*att,*sflag;
long pos;
{
   int i,retries;
   int newfile;
   char buff[150];
   bool_t result = FALSE;
   mode_t perms = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
   retries = 0;
   try_update:
   if (symlink("/usr/security/pwdfile","/tmp/pwdfile.lock") != 0) {
   sleep(1);
   retries++;
   if (retries > 30) {
      printf("\n\nUnable to update accounting file.\n");
      printf("\nPlease contact your system administrator.\n");
      dropline();
      exit(0);
   }
   goto try_update;
  }
   
   if ((newfile = creat(PF_NEWFILE, perms)) != -1) {
      pwdfile=(FILE *)fopen("/usr/security/pwdfile","r+");
      rewind(pwdfile);

      while (!feof(pwdfile)) {
         if (fgets(buff, sizeof(buff), pwdfile) != NULL) {
            if (strncmp(buff, usr, strlen(usr)) == 0 &&
                buff[strlen(usr)] == ':') { 
                //Checks the next character in the buffer for a :.  If there is a : then we are on
                //the right user.  If there is no : that means the user name is longer than what we
                //are looking for.  This avoids the issue of tom resetting toms password.
               sprintf(buff, "%s:%s:%s:%s:%s:%s:%s:%s\n", usr,
                  pwd, sstr, lcode, la, lc, att, sflag);
               if(write(newfile, buff, strlen(buff)) == -1) {
                  result = FALSE;
               }
               else {
                  result = TRUE;
               }
            }
            else {
               if (write(newfile, buff, strlen(buff)) == -1) {
                  result = FALSE;
               }
            }
         }
      }

      close(newfile);

      if (result == TRUE) {
         fclose(pwdfile);
         chmod(PF_NEWFILE, perms);
         chown(PF_NEWFILE, (uid_t)0, ws_groupid());
         rename(PF_NEWFILE, PF_FILENAME);
         pwdfile = fopen(PF_FILENAME, "r");
      }
      else {
         remove(PF_NEWFILE);
      }
   }
   
   unlink("/tmp/pwdfile.lock");
   return (0);
}

long findusr(usrname)
char *usrname;
{
long cfpos;
char buff[150],inname[10];
int i,bufflen;
cfpos=ftell(pwdfile);
while (fgets(buff,sizeof(buff),pwdfile) != NULL) {
   i = 0;
   bufflen = strlen(buff);
   while ((buff[i] != ':') && (i < bufflen)) {
      inname[i] = buff[i];
      i++;
   }
   inname[i] = 0;
   if (strcmp(inname,usrname) == 0) return(cfpos);
   cfpos=ftell(pwdfile);
}
cfpos = -1;
return(cfpos);
}

pwdchange(pwd,flag)
char *pwd;
int flag;
{
char pwd1[30],pwd2[30];
char *encpwd;
regex_t re;
regmatch_t rm;
regex_t re_lc;
regmatch_t rm_lc;
regex_t re_uc;
regmatch_t rm_uc;
regex_t re_sc;
regmatch_t rm_sc;
regex_t re_num;
regmatch_t rm_num;



char *pat = "wesco";
char *lc_pat = "[a-z]";
char *uc_pat = "[A-Z]";
char *sc_pat = "[^a-zA-Z0-9 ]";
char *num_pat = "[0-9]";
int num_match;
if (flag == 0){
    printf("\nYour current password has expired -- Please enter a new one\n");
}
else{
    printf("\nYou may now change your password -- Please enter a new one\n");
}

regcomp(&re, pat, REG_ICASE);
regcomp(&re_lc, lc_pat, REG_EXTENDED);
regcomp(&re_uc, uc_pat, REG_EXTENDED);
regcomp(&re_sc, sc_pat, REG_EXTENDED);
regcomp(&re_num, num_pat, REG_EXTENDED);

getnpwd:

num_match = 0;
printf("New Password:");
getpwd(pwd1);


if (strlen(pwd1) < 9 || strlen(pwd1) > 15) {
   printf("\nYour new password must be between 9 and 15 characters long.\n");
   printf("Please Re-enter.\n");
   goto getnpwd;
}

/* We need to check pwd1 for the presence of the string
   "wesco", "Wesco", or "WESCO". If the pwd contains any of
   these, we will reject it, displaying an error message. Then
   we will goto (ugh) the password prompt to let the user try
   again. */

if(regexec(&re, pwd1, (size_t)1, &rm, 0) == 0)
{
  printf("\nInvalid Password -- Passwords cannot contain the word wesco\n"); 
  goto getnpwd;
}

encpwd = crypt(pwd1,"$1$W2$");
if (strcmp(encpwd,pwd) == 0) {
   printf("\nYou new password cannot be the same as your old one!\n");
   printf("Please Re-enter.\n");
   goto getnpwd;
}

if(regexec(&re_lc, pwd1, (size_t)1, &rm_lc, 0) == 0)
{
  num_match++;
}

if(regexec(&re_uc, pwd1, (size_t)1, &rm_uc, 0) == 0)
{
  num_match++;
}

if(regexec(&re_sc, pwd1, (size_t)1, &rm_sc, 0) == 0)
{
  num_match++;
}

if(regexec(&re_num, pwd1, (size_t)1, &rm_num, 0) == 0)
{
  num_match++;
}

if(num_match < 3)
{
  printf("\nInvalid Password -- Passwords must contain 3 of the 4 following characters\n");
  printf("1) at least 1 lower case letter\n");
  printf("2) at least 1 upper case letter\n");
  printf("3) at least 1 special character\n");
  printf("4) at least 1 number\n");
   printf("Please Re-enter.\n");
   goto getnpwd;
}

printf("\nPlease verify you new password by re-entering it.\n");
printf("Verify new password:");
getpwd(pwd2);
if (strcmp(pwd1,pwd2) != 0) {
   printf("Passwords don't match!!!\n");
   printf("Please Re-enter.\n");
   goto getnpwd;
}
strcpy(pwd,pwd1);
regfree(&re);
regfree(&re_num);
regfree(&re_sc);
regfree(&re_uc);
regfree(&re_lc);

return(0);
}

void cleanup()
{
unlink("/tmp/pwdfile.lock");
exit(1);
}
