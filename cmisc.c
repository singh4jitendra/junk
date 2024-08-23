/* print routines for client - adds timestamp */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#ifdef DOS
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "cdial.h"

extern int errno;

char * stripline(s) 
char *s;
{
   char *l;

   l = strstr(s,"term/");
   if (l!=NULL)
      l+=5;   /* skip to just past 'term' in the name. */
   else
      l=s;

   return l;
}

void noretry(struct DIAL_PARMS *p)
{
   p=p;
}

void retry(struct DIAL_PARMS *p)
{
   p=p;
}

void perror_out(p,s,i)
struct DIAL_PARMS *p;
char *s;
int i;
{
   tfprintf(stdout,"LINE: %s ERROR: %s: (%d)%s\n",
      stripline(p->line),s,i,strerror(i));
}

void pinfo(struct DIAL_PARMS *p, char *s)
{
   tfprintf(stdout,"LINE: %s %s",stripline(p->line),s);
}

/*
 Like printf, except a time stamp is added to the front of the message
*/
int tprintf(va_alist)
va_dcl 
{
   time_t ltime;
   struct tm *tm;
   va_list args;
   char *fmt;
   int i;

   ltime = time(NULL);
   tm = localtime(&ltime);

   i = printf("%02u/%02u/%02u %02u:%02u:%02u ",
      tm->tm_year%100,tm->tm_mon+1,tm->tm_mday,
      tm->tm_hour,tm->tm_min,tm->tm_sec);

   va_start(args);
   fmt = va_arg(args, char *);

   i += vprintf(fmt, args);
   fflush(stdout);

   return i; 
}

/*
 Like printf, except a time stamp is added to the front of the message
*/
int tprintfo(va_alist)
va_dcl 
{
time_t ltime;
struct tm *tm;
va_list args;
char *fmt;
int i;

ltime = time(NULL);
tm = localtime(&ltime);

i = printf("%02u/%02u/%02u %02u:%02u:%02u ",
tm->tm_year%100,tm->tm_mon+1,tm->tm_mday,
tm->tm_hour,tm->tm_min,tm->tm_sec);

va_start(args);
fmt = va_arg(args, char *);

i += vprintf(fmt, args);
fflush(stdout);

return i; 
}

/*
 A version of printf, with a different name, so that a real printf
 can be called even if a #define of printf is used.
*/
int nprintf(va_alist)
va_dcl 
{
   va_list args;
   char *fmt;

   va_start(args);
   fmt = va_arg(args, char *);

   return vprintf(fmt, args);
}

/*
 A version of fprintf, with a different name, so that a real printf
 can be called even if a #define of printf is used.
*/
int nfprintf(va_alist)
va_dcl 
{
   va_list args;
   char *fmt;
   FILE *fp;

   va_start(args);
   fp = va_arg(args, FILE *);
   fmt = va_arg(args, char *);

   return vfprintf(fp,fmt, args);
}

/*
 Like fprintf, except a time stamp is added to the front of the message
*/
int tfprintf(va_alist)
va_dcl
{
   time_t ltime;
   struct tm *tm;
   int i;
   va_list args;
   FILE *fp;
   char *fmt;
   char buf[120];

   va_start(args);
   fp = va_arg(args, FILE *);
   fmt = va_arg(args, char *);

   ltime = time(NULL);
   tm = localtime(&ltime);

   i = fprintf(fp,"%02u/%02u/%02u %02u:%02u:%02u ",
      tm->tm_year%100,tm->tm_mon+1,tm->tm_mday,
      tm->tm_hour,tm->tm_min,tm->tm_sec);

   i += vfprintf(fp,fmt, args);
   fflush(fp);

   return i;
}

int check_out(path)
char *path;
{
   DIR *dir;
   struct dirent *dp;
   int count;

   if ((dir = opendir(path)) == NULL) {
      return -1;
   }

   closedir(dir);

   return 0;
}
