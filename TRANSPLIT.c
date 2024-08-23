#include <stdio.h>
#include <string.h>
#include <fcntl.h>

TRANSPLIT(fname,buff,blength,command)
char *fname,*blength,*buff,*command;
{
int outlen;
int i;
static int ofile;
char ofilename[9],hlen[4];
if (command[0] == 'O') {
   strncpy(ofilename,fname,8); ofilename[8]='\0';
   ofile = open(ofilename, O_WRONLY | O_CREAT | O_TRUNC);
   return(0);
}
if (command[0] == 'C') {
   close(ofile);
   return(0);
}
if (command[0] == 'W') {
   strncpy(hlen,blength,3);
   outlen = atoi(hlen);
   write(ofile,buff,outlen);
}
return(0);
}
