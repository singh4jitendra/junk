#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <termio.h>

int oterm,debug,intnum;
FILE *SFILE,*IN;

main(argc,argv)
int argc;
char *argv[];
{
int pid,rstats,i,tval;
int catcher();
char buff[200],cmd[200];
struct termio termctl;
debug = 0;
if ((oterm=open(argv[1],O_RDWR | O_NDELAY)) == -1) {
   printf("Open failled for output file \n");
   exit(2);
}
if ((IN=(FILE *)fopen(argv[2],"r")) == NULL) {
   printf("Open failled for input file \n");
   exit(1);
}
pid=getpid();
signal(SIGHUP,(void(*)(int))catcher);
termctl.c_iflag = (IGNBRK | INPCK |IXON | IXOFF | 0040000);
termctl.c_oflag = CR1;
termctl.c_cc[4] = 1;
termctl.c_cc[5] = 1;
termctl.c_cflag = (B2400 | CS7 | CREAD | PARENB | HUPCL);
termctl.c_lflag = ISIG;
errno = 0;
rstats=ioctl(oterm,TCSETA,&termctl);
tval = 60;
signal(SIGALRM,(void(*)(int))catcher);
read_next:
if (fgets(cmd,200,IN) == NULL) goto done_read;
cmd[strlen(cmd)-1] = 0;
if (strncmp(cmd,"reply",5) == 0) {
   getpat(cmd,buff,5);
   alarm(tval);
   write(oterm,buff,strlen(buff));
   if (errno == 14) {
      printf("timeout error sending %s\n",buff);
      do_suicide();
      exit(1);
   }
   alarm(0);
   goto read_next;
}
if (strncmp(cmd,"wait",4) == 0) {
   getpat(cmd,buff,4);
   if (getstr(buff,strlen(buff),tval) == -1) {
      printf("Timeout waiting for %s\n",buff);
      exit(1);
   }
   goto read_next;
}
if (strncmp(cmd,"send",4) == 0) {
   getpat(cmd,buff,4);
   if (do_send(buff,tval) == -1) goto done_read;
   goto read_next;
}
if (strncmp(cmd,"delay",5) == 0) {
   do_delay(cmd);  
   goto read_next;
}
if (strncmp(cmd,"timeout",7) == 0) {
   getpat(cmd,buff,7);
   tval = atoi(buff);
   if (tval == 0) tval = 60;
   goto read_next;
}
printf("bad command - %s\n",cmd);
goto read_next;
done_read:
signal(SIGHUP,SIG_DFL);
close(oterm);
fclose(IN);
exit(0);
}

do_delay(cmd)
char *cmd;
{
int deltime;
char chrdel[200];
 
getpat(cmd,chrdel,5);
deltime = atoi(chrdel);
sleep(deltime);
}
 
do_send(fname,tval)
int tval;
char *fname;
{
char sbuff[200];
char inbuff[5], cdrop[12];
 
if ((SFILE = (FILE *)fopen(fname,"r")) == NULL) {
    perror("do_send");
    return(0);
}
errno = 0;
strcpy(cdrop,"O CARRIER");
while (fgets(sbuff,200,SFILE) != NULL) {
    sbuff[strlen(sbuff)-1] = 0;
    strcat(sbuff,"\r\n");
    intnum = 0;
    tval = 15;
    alarm(tval);
    errno = 0;
    write(oterm,sbuff,strlen(sbuff));
    if ((read(oterm,inbuff,1)) != 0) {
       if (getstr(cdrop,9,5) == 0) {
           printf("line dropped\n");
           do_suicide();
           exit(1);
       }
    }
    if (intnum == 14) {
        printf("Timeout error sending file %s\n",fname);
        do_suicide();
        exit(1);
    }
    alarm(0);
}
fclose(SFILE);
printf("sent %s\n",fname);
sleep(10);
return(0);
}
    
getpat(cmd,buff,ignoref)
char *cmd,*buff;
int ignoref;
{
int i,j;

j = 0;
for (i=ignoref;i<strlen(cmd);i++) {
  if (cmd[i] != ' ') goto start_cpy;
}
buff[0] = 0;
return(0);
start_cpy:
while (i <= strlen(cmd)) {
   switch (cmd[i]) {
     case '|':  buff[j] = '\r';
                break;
     case '!':  buff[j] = '\r';
                j++;
                buff[j] = '\n';
                break;
     case '%':  buff[j] = 12;
                break;
     default:   buff[j] = cmd[i];
                break;
    }
   i++;
   j++;
}
buff[j] = 0;
}
   
getstr(buff,blen,timeval)
int blen,timeval;
char *buff;
{
int i,ptr1,ptr2,rcode;
char inbuff[200];
long int ctime;
ctime = time((long *) 0);
ptr1 = 0;
ptr2 = 0;
while ((ctime + timeval) > time((long *) 0)) {
   rcode = read(oterm,inbuff+ptr1,1);
   if (rcode == 0) {
      sleep(1);
   }
   else {
      if (rcode == -1) {
         perror("");
         exit(1);
      }
      if (inbuff[ptr1] == buff[ptr2]) {
          ptr1++;
          ptr2++;
          if (ptr2 == blen) return(0);
      }
      else {
          ptr1 = 0;
          ptr2 = 0;
      }
   }
}
return(-1);
}

int catcher(sig)
int sig;
{
intnum = sig;
if (sig == 1) {
  printf("exitting - line drop\n");
  do_suicide();
  exit(1);
}
return(0);
}
 
do_suicide()
{
fclose(IN);
fclose(SFILE);
ioctl(oterm,TCFLSH,2);
close(oterm);
}
