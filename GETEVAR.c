#include <stdio.h>
#include <string.h>
int GETEVAR(vname,vcontents) 
char *vname,*vcontents;
{
char *getenv(),*nval;
char buff[81];
int i;
strncpy(buff,vname,80);
for (i=80;i>0;i--) {
  if (buff[i] == ' ') {
     buff[i] = 0;
  }
}
if ((nval = getenv(buff)) == NULL) {
   for (i=0;i<80;i++) vcontents[i] = ' ';
   return(0);
}
strncpy(vcontents,nval,80);
for (i=strlen(vcontents);i<80;i++) vcontents[i] = ' ';
return(0);
}
