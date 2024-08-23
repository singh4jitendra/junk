#ident "@(#)  secmaint.c  Version 1.3  Date Retreived  8/13/92"

/*
 ******************************************************************************
 * 02/16/06 mjb 05078 - Modified scripts to call library routines to access
 *                      /usr/security/pwdfile.
 * 02/20/06 mjb 05078 - Modified to code not set a new password if the "l"
 *                      option is used with the "-c" option.
 * 02/23/06 mjb 05078 - Defect #471. The program generated a memory fault when
 *                      it attempted to modify a user that did not exist on 
 *                      pwdfile.
 ******************************************************************************
*/
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <time.h>
#include <unistd.h>
#include "ws_pwfile.h"
#include <stdlib.h>

char dtstamp[10];
char *wpasswd; 

main(argc,argv)
int argc;
char *argv[];
{
int i,j;
char startstr[50],username[50],langcode[10];
long currtime;
struct tm *currday;
void adduser(),deluser(),resetusr(),chgpwd();
char *crypt();
char passwd[50];

if (argc == 1) exit(0);
currtime = time((long *) 0);
currday = gmtime(&currtime);
currday->tm_year %= 100;    /* insure 2 digit day, not 3 digit */
sprintf(dtstamp,"%02d%03d",currday->tm_year,currday->tm_yday);
wpasswd = crypt("wescom2","$1$W2$");
 
if ((argv[1][0] == '-') && (argc > 2)) {
  strcpy(username,argv[2]);
  startstr[0] = 0;
  passwd[0] = 0;
  langcode[0] = 0;
  if ((argv[1][2] == 'l') && (argc > 3)) 
     strcpy(langcode,argv[3]);
  else
     if (argc > 4) 
        strcpy(langcode,argv[4]);
  if (langcode[0]==0){
	langcode[0] = ' ';
	langcode[1] = 0;
	}
  switch(argv[1][1]) {
    case 'a': case 'A':
      if ((argc > 3) && (argv[1][2] != 'l')) strcpy(startstr,argv[3]);
      adduser(username,startstr,langcode);
      break;
    case 'c': case 'C':
      if ((argc > 3) && (argv[1][2] != 'l')) strcpy(passwd,argv[3]);
      chgpwd(username,passwd,langcode);
      break;
    case 'd': case 'D':
      deluser(username);
      break;
    case 'r': case 'R':
      resetusr(username);
      break;
    default:
      exit(1);
   }
}
exit(0);
}

void adduser(char * usrname, char * startstr, char * langcode)
{
   struct pf_entry user;

   ws_pfopen();
   if (ws_get_pfentry_by_name(usrname) == NULL) {
      strncpy(user.pf_name, usrname, sizeof(user.pf_name));
      strncpy(user.pf_passwd, wpasswd, sizeof(user.pf_passwd));

      if (*startstr != 0) {
         strncpy(user.pf_start, startstr, sizeof(user.pf_start));
      }

      user.pf_langcode = *langcode;
      strncpy(user.pf_lastaccess, dtstamp, sizeof(user.pf_lastaccess));
      strncpy(user.pf_lastchange, "50000", sizeof(user.pf_lastchange));
      user.pf_attempts = 0;
      user.pf_sflag = ' ';
      ws_update_pf_file(&user, PF_ADDUSER);
      ws_pfclose();
   }
   else {
      ws_pfclose();
      exit(3);
   }
}

void deluser(char * usrname)
{
   struct pf_entry * pfe;
   
   ws_pfopen();

   pfe = ws_get_pfentry_by_name(usrname);
   if (pfe != NULL) {
      ws_update_pf_file(pfe, PF_DELUSER);
   }

   ws_pfclose();
}

void resetusr(char * usrname) 
{
   struct pf_entry * pfe;

   ws_pfopen();

   pfe = ws_get_pfentry_by_name(usrname);
   if (pfe != NULL) {
      strncpy(pfe->pf_passwd, wpasswd, sizeof(pfe->pf_passwd));
      strncpy(pfe->pf_lastaccess, dtstamp, sizeof(pfe->pf_lastaccess));
      strncpy(pfe->pf_lastchange, "50000", sizeof(pfe->pf_lastchange));
      pfe->pf_attempts = 0;
      ws_update_pf_file(pfe, PF_UPDUSER);
   }

   ws_pfclose();
}

void chgpwd(char * usrname, char * newpasswd, char * langcode) 
{
   struct pf_entry * pfe;
   char * npasswd = crypt(newpasswd, "$1$W2$");

   ws_pfopen();
   if ((pfe = ws_get_pfentry_by_name(usrname)) == NULL) {
      ws_pfclose();
      exit(7);
   }

   if (newpasswd[0] != 0) {
      strncpy(pfe->pf_passwd, npasswd, sizeof(pfe->pf_passwd));
   }

   strncpy(pfe->pf_lastaccess, dtstamp, sizeof(pfe->pf_lastaccess));
   strncpy(pfe->pf_lastchange, "50000", sizeof(pfe->pf_lastchange));
   pfe->pf_attempts = 0;
   ws_update_pf_file(pfe, PF_UPDUSER);
   ws_pfclose();
}
