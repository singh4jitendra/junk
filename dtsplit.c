#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ident "@(#)Module: dtsplit.c  Release: 1.1  Last changed: 6/15/94"	

char dtable[100][3];

main(argc,argv)
int argc;
char *argv[];
{

char tfname[30];
char tabname[30],dtname[30],eodname[30];
FILE *tranfile,*tabfile;
FILE *holdfile,*procfile;
char buff[512],sumchar[10];
int i,j,tablength;
int dtlines,eodlines,sumlines;
union {
  struct {
    char fill1[10];
    char tcode[3];
    union {
      char fill2[187];
      struct {
       char fill4[23];
       char chksum[8];
       char fill3[156];
      } sumfld;
    } rec;
  } inrec;
  char inbuff[512];
} rarea;

if (argc < 3) exit(1);
strcpy(tfname,argv[2]);
sprintf(tabname,"/usr/hlcp/dttables/%s.tbl",argv[1]);
strcpy(dtname,tfname);
strcat(dtname,"D");
strcpy(eodname,tfname);
strcat(eodname,"E");
dtlines = eodlines = 0;

/*   We still have to figure out what to do if table does not exist  */

if ((tabfile = (FILE *)fopen(tabname,"r")) == NULL) exit(0);

if ((tranfile = (FILE *)fopen(tfname,"r")) == NULL) exit(2);

if ((holdfile = (FILE *)fopen(eodname,"w")) == NULL) exit(2);

if ((procfile = (FILE *)fopen(dtname,"w")) == NULL) exit(2);

i = 0;
while(fgets(buff,512,tabfile) != NULL) {
   nlstrip(buff);
   strcpy(dtable[i],buff);
   i++;
}
fclose(tabfile);
tablength = i;

while(fgets(rarea.inbuff,512,tranfile) != NULL) {
   j = 0;
   if (strcmp(argv[1],"Z999S") == 0) 
      if (strncmp(rarea.inrec.tcode,"997",3) == 0) {
         strncpy(sumchar,rarea.inrec.rec.sumfld.chksum,8);
         sumchar[8] = 0;
         sumlines=atoi(sumchar);
         if (eodlines+dtlines != sumlines) {
            fclose(tranfile);
            fclose(procfile);
            fclose(holdfile);
            unlink(eodname);
            unlink(dtname);
            exit(5);
         }
         fprintf(procfile,"%36.36s%08d\n",rarea.inbuff,dtlines);
         fprintf(holdfile,"%36.36s%08d\n",rarea.inbuff,eodlines);
         break;
   }
   for (i=0;(i<=tablength) && (j==0);i++) 
      if (strncmp(rarea.inrec.tcode,dtable[i],3) == 0) j=1;
   if (j == 1) {
      fprintf(procfile,"%s",rarea.inbuff);
      dtlines++;
   }
   else {
      fprintf(holdfile,"%s",rarea.inbuff);
      eodlines++;
   }
}
fclose(tranfile);
fclose(procfile);
fclose(holdfile);
if (eodlines == 0) unlink(eodname);
}

nlstrip(instr)
char *instr;
{
int i;
for (i=0;i<strlen(instr);i++) {
   if (instr[i] == 10) instr[i] = 0;
}
}
