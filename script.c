#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cdial.h"
#include "line.h"

/* HEP 12/9/94  - 
               call get_dcd in script.c before each operation, to cause
               the line to cycle if DCD has bobbled - new USR modems seem
               to diddle DCD on ATZ.
*/


extern int errno;


void ati6_out(struct DIAL_PARMS *,char *);

int do_script(struct DIAL_PARMS *p, int dir)
{
   FILE *scriptfp;
   char linebuf[256];
   int err,len;

   int dtr_is_up=0;

   if (p->script==NULL)
      scriptfp = fopen(DEFAULT_SCRIPT,"r");
   else
      scriptfp = fopen(p->script,"r");

   if (scriptfp==NULL)
      return errno;

   while (fgets(linebuf,sizeof(linebuf),scriptfp)!=NULL) {
      if (strncmp(linebuf,"s:",2)==0)  {
         if (dir!=SCRIPT_INIT)
            continue;

         sleep(1);
         get_dcd(p);   /* hep 12/9/94 - avoid USR problem with DCD */
         sleep(1);
         if(linebuf[strlen(linebuf)-1] == '\n') 
            linebuf[strlen(linebuf)-1] = '\r';

         if (err=send_line(p,linebuf+2, strlen(linebuf)-2))
            return err;
      }
      else if (strncmp(linebuf,"r:",2)==0)  {
         if (dir!=SCRIPT_INIT)
            continue;

         if (linebuf[strlen(linebuf)-1] == '\n') 
            linebuf[strlen(linebuf)-1] = '\r';

         if (err=get_line(p,linebuf+2, strlen(linebuf)-2,TIMEOUT))
            return err;
         else if (p->debug>8)
            printf("\n");
      }
      else if (strncmp(linebuf,"e:",2)==0)  {
         if (dir!=SCRIPT_EXIT)
            continue;

         if (!dtr_is_up) {
            dtr_up(p);
            sleep(1);
            flush_line(p);
            dtr_is_up=1;
         }

         if (linebuf[strlen(linebuf)-1] == '\n') 
            linebuf[strlen(linebuf)-1] = '\r';

         if (err=send_line(p,linebuf+2, strlen(linebuf)-2))
            return err;
      }
      else if (strncmp(linebuf,"x:",2)==0)  {
         if (dir!=SCRIPT_EXIT)
            continue;

         if (linebuf[strlen(linebuf)-1] == '\n') 
            linebuf[strlen(linebuf)-1] = '\r';

         if (err=get_line(p,linebuf+2, strlen(linebuf)-2,TIMEOUT))
            return err;
         else if (p->debug>8)
            printf("\n");
      }
      else if (strncmp(linebuf,"getati6",7)==0)  {
         if (dir!=SCRIPT_EXIT)
            continue;

         if (!dtr_is_up) {
            dtr_up(p);
            sleep(1);
            flush_line(p);
            dtr_is_up=1;
         }

         if (err=send_line(p,"ATI6\r", 5))
            return err;

         while (1) {
            if ((len = get_line_buf(p,linebuf, sizeof(linebuf)-1,TIMEOUT))<0)
               return len;
            else {
               if (strstr(linebuf,"\nOK\r")!=NULL)
                  break;

               linebuf[len]='\0';
               ati6_out(p,linebuf); 
            }
         }
      }
      else if (*linebuf=='#')
         continue;
      else if (*linebuf!='\n') {
         fclose(scriptfp);
         return BAD_SCRIPT;      /* bad script file */
      }
   }

   fclose(scriptfp);
   return 0;
}

void ati6_out(p,s)
struct DIAL_PARMS *p;
char *s;
{
   int i;
   int len;

   len=strlen(s)-1;      /* scan all but notional last <lf> */
   for (i=0;i<len;i++) {
      if (s[i]=='\r')
         s[i]='%';
      else if (s[i]=='\n')
         s[i]='~';
   }
 
   if (s[i]=='\r')
      s[i]='%';      /* get trailing \r */

   tfprintf(stdout,"LINE: %s ATI6: %s\n",stripline(p->line),s);
}
