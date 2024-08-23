/*
 ******************************************************************************
 * This file contains functions that provide access to the WESNET security
 * file, /usr/security/pwdfile.
 *
 * History:
 * 09/09/05 mjb 05078 - Initial development.
 * 02/16/06 mjb 05078 - Modified pwdfile update logic to create a temporary
 *                      file and then rename it.
 * 11/01/06 mjb 05078 - Defect 1669. Rolled forward from version 10.00.00.
 *                      Added logic to set the language code when a user logs
 *                      in to the system.
 ******************************************************************************
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <grp.h>
#include <regex.h>
#include "ws_pwfile.h"
#include "msdefs.h"

int pf_errno;
fpos_t filePosition;
FILE * fp = NULL;

void ws_pfclose()
{
   if (fp != NULL) {
      fclose(fp);
      ws_pfunlock();
      fp = NULL;
   }
}

int ws_getlastpferror()
{
   return pf_errno;
}

int ws_pfopen()
{
   int rv = 0;

   pf_errno = PF_SUCCESS;
   if (fp == NULL) {
      if (ws_pflock() == 0) {
         if ((fp = fopen(PF_FILENAME, "r")) == NULL) {
	    ws_pfunlock();
	    rv = -1;
	 }
      }
      else {
         rv = -1;
     }
   }

   return rv;
}

struct pf_entry * ws_get_pfentry_by_name(const char * name)
{
   char * start;
   char * end;
   char buffer[255];
   static struct pf_entry pfe;
   
   pf_errno = PF_SUCCESS;
   while (!feof(fp)) {
      fgetpos(fp, &filePosition);
      fgets(buffer, sizeof(buffer), fp);
      memset(&pfe, 0, sizeof(struct pf_entry));
      if (*buffer != '#') {
         /* parse out the login name */
         start = strtok(buffer, ":\n");
         if (start != NULL) {
            strncpy(pfe.pf_name, start, sizeof(pfe.pf_name));
         }
         else {
            continue;
         }
         
         if (strcmp(pfe.pf_name, name) == 0) {
            /* parse out the user password */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
               strncpy(pfe.pf_passwd, start, sizeof(pfe.pf_passwd));
            }
            else {
               return NULL;
            }
            
            /* parse out the user shell */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
               strncpy(pfe.pf_start, start, sizeof(pfe.pf_start));
            }
            else {
               return NULL;
            }
            
            /* parse out the language code */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
               pfe.pf_langcode = *start;
            }
            else {
               return NULL;
            }
            
            /* parse out the last access date */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
/*               pfe.pf_lastaccess = ws_calculate_pf_date(start); */
               strncpy(pfe.pf_lastaccess, start, sizeof(pfe.pf_lastaccess));
            }
            else {
               return NULL;
            }
            
            /* parse out the last password change date */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
/*               pfe.pf_lastchange = ws_calculate_pf_date(start); */
               strncpy(pfe.pf_lastchange, start, sizeof(pfe.pf_lastchange));
            }
            else {
               return NULL;
            }
            
            /* parse out the attempts */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
               pfe.pf_attempts = atol(start);
            }
            else {
               return NULL;
            }
            
            /* parse out the supervisor flag */
            start = strtok(NULL, ":\n");
            if (start != NULL) {
               pfe.pf_sflag = *start;
            }
            else {
               pfe.pf_sflag = ' ';
            }

            return &pfe;
         }
      }
   }

   return NULL;
}

bool_t ws_authenticate_login(const char * name, const char * pwd)
{
   char * encryptedPwd;
   char salt[PF_SALT_MAX + 1];
   struct pf_entry * pfe;
   int authenticated = FALSE;
   pfdate_t currentDate = ws_get_current_pf_date();
   pfdate_t lastAccess;
   pfdate_t lastChange;
   char today[6];
   char langcode[2];
   
   pf_errno = PF_SUCCESS;
   pfe = ws_get_pfentry_by_name(name);
   if (pfe != NULL) {
      lastAccess = ws_calculate_pf_date(pfe->pf_lastaccess);
      lastChange = ws_calculate_pf_date(pfe->pf_lastchange);
      memset(salt, 0, sizeof(salt));
      strncpy(salt, pfe->pf_passwd, PF_SALT_MAX);
      encryptedPwd = crypt(pwd, salt);
      if  ((pfe->pf_attempts >= PF_ATTEMPTS_MAX) ||
          (((lastAccess + 90) < currentDate) &&
          pfe->pf_sflag != 'Y')) {
             pf_errno = PF_USER_LOCKED;
      }
      else if (strcmp(pfe->pf_passwd, encryptedPwd) == 0) {
         /* the user entered the correct password, is it expired? */
         if (((lastChange + 90) < currentDate) && (pfe->pf_sflag != 'Y')) {
            pf_errno = PF_PASSWORD_EXPIRED;
         }
         else {
            ws_format_pf_today(pfe->pf_lastaccess);
            pfe->pf_attempts = 0;
            ws_update_pf_file(pfe, PF_UPDUSER);
            fsetpos(fp, &filePosition);
            authenticated = TRUE;
            memset(langcode, 0, sizeof(langcode));
            langcode[0] = pfe->pf_langcode;
            setenv("LANGCODE", langcode, 1);
         }
      }
      else {
         pfe->pf_attempts++;
         ws_update_pf_file(pfe, PF_UPDUSER);
         fsetpos(fp, &filePosition);
         pf_errno = PF_INVALID_PASSWORD;
      }
   }
   else {
      pf_errno = PF_INVALID_USER;
   }
   
   return authenticated;
}

bool_t ws_set_password(const char * name, const char * pwd)
{
   int reset = FALSE;
   size_t length;
   struct pf_entry * pfe;
   char * salt = "$1$W2$";
   char * encryptedPwd;
   char today[6];

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
   int num_match = 0;

   pf_errno = PF_SUCCESS;
   regcomp(&re, pat, REG_ICASE);
   regcomp(&re_lc, lc_pat, REG_EXTENDED);
   regcomp(&re_uc, uc_pat, REG_EXTENDED);
   regcomp(&re_sc, sc_pat, REG_EXTENDED);
   regcomp(&re_num, num_pat, REG_EXTENDED);
   
   /* The user must be authenticated before the password is changed. */
   length = strlen(pwd);
   if ((length < 9 ) || (length > 15)) {
      pf_errno = PF_PASSWORD_LENGTH;
   }
   else if (!regexec(&re,pwd,(size_t)1,&rm,0)) {
   /* Check the password for the string "wesco", case insensitive */
   /* If we find a match, return an error message                 */
      pf_errno = PF_ILLEGAL_PASSWORD;  
   }
   else {
      if(regexec(&re_lc, pwd, (size_t)1, &rm_lc, 0) == 0) {
        num_match++;
      }
      if(regexec(&re_uc, pwd, (size_t)1, &rm_uc, 0) == 0) {
        num_match++;
      }
      if(regexec(&re_sc, pwd, (size_t)1, &rm_sc, 0) == 0) {
        num_match++;
      }
      if(regexec(&re_num, pwd, (size_t)1, &rm_num, 0) == 0) {
        num_match++;
      }
      if(num_match < 3) {
         pf_errno = PF_UNCOMPLEX_PASSWORD;
      }
      else {
         fsetpos(fp, &filePosition);
         pfe = ws_get_pfentry_by_name(name);
         fsetpos(fp, &filePosition);
         encryptedPwd = crypt(pwd, salt);
         strncpy(pfe->pf_passwd, encryptedPwd, sizeof(pfe->pf_passwd));
         ws_format_pf_today(pfe->pf_lastaccess);
         strncpy(pfe->pf_lastchange, pfe->pf_lastaccess,
            sizeof(pfe->pf_lastchange));
         pfe->pf_attempts = 0;
         ws_update_pf_file(pfe, PF_UPDUSER);
         reset = TRUE;
      }
   }

   regfree(&re);
   regfree(&re_num);
   regfree(&re_sc);
   regfree(&re_uc);
   regfree(&re_lc);

   return reset;
}

bool_t ws_reset_password(const char * name, const char * pwd)
{
   bool_t reset;

   pf_errno = PF_SUCCESS;
   if (getuid() == 0) {
      reset = ws_set_password(name, pwd);
   }
   else {
      pf_errno = PF_PERMISSION_DENIED;
      reset = FALSE;
   }

   return reset;
}

bool_t ws_chg_pwd(const char * name, const char * oldpwd, const char * newpwd)
{
   bool_t changed = FALSE;

   pf_errno = PF_SUCCESS;
   if (ws_authenticate_login(name, oldpwd) == TRUE ||
       pf_errno == PF_PASSWORD_EXPIRED) {
      if (strcmp(newpwd, oldpwd) == 0) {
         pf_errno = PF_EXISTING_PASSWORD;
      }
      else {
         changed = ws_set_password(name, newpwd);
      }
   }

   return changed;
}

pfdate_t ws_get_current_pf_date(void)
{
   time_t currentTime;
   struct tm * universalTime;
   
   currentTime = time(NULL);
   universalTime = gmtime(&currentTime);
   if (universalTime->tm_year < 50) universalTime->tm_year += 100;
   return (pfdate_t)((universalTime->tm_year * 365) + universalTime->tm_yday);
}

void ws_format_pf_today(char * buffer)
{
   time_t currentTime;
   struct tm * universalTime;

   currentTime = time(NULL);
   universalTime = gmtime(&currentTime);
   sprintf(buffer, "%02d%03d", universalTime->tm_year % 100,
      universalTime->tm_yday);
}

pfdate_t ws_calculate_pf_date(const char * pfdate)
{
   long date;
   char year[3];
   
   memset(year, 0, sizeof(year));
   strncpy(year, pfdate, sizeof(year) - 1);
   date = atol(year);
   if (date < 50) {
      date += 100;
   }
   
   return (pfdate_t)((date * 365) + atol(pfdate + 2));
}

struct pf_entry * ws_getpfent(void)
{
   char buffer[255];
   static struct pf_entry pfe;

   while (!feof(fp)) {
      fgetpos(fp, &filePosition);
      fgets(buffer, sizeof(buffer), fp);
      if (*buffer != '#') {
         memset(&pfe, 0, sizeof(struct pf_entry));
         if (ws_parse_pf_ent(buffer, &pfe)) {
            return &pfe;
         }
      }
   }

   return NULL;
}

void ws_setpfent(void)
{
   rewind(fp);
   fgetpos(fp, &filePosition);
}

bool_t ws_parse_pf_ent(char * buffer, struct pf_entry * pfe)
{
   char * start;

   if ((start = strtok(buffer, ":\n")) != NULL) {
      strncpy(pfe->pf_name, start, sizeof(pfe->pf_name));
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      strncpy(pfe->pf_passwd, start, sizeof(pfe->pf_passwd));
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      strncpy(pfe->pf_start, start, sizeof(pfe->pf_start));
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      pfe->pf_langcode = *start;
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      strncpy(pfe->pf_lastaccess, start, sizeof(pfe->pf_lastaccess));
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      strncpy(pfe->pf_lastchange, start, sizeof(pfe->pf_lastchange));
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      pfe->pf_attempts = atol(start);
   }
   else {
      return FALSE;
   }

   if ((start = strtok(NULL, ":\n")) != NULL) {
      pfe->pf_sflag = *start;
   }
   else {
      pfe->pf_sflag = ' ';
   }

   return TRUE;
}

int ws_pflock(void)
{
   int retries = 0;

   pf_errno = PF_SUCCESS;

   do {
      if (symlink(PF_FILENAME, PF_LOCKFILE) != 0) {
         sleep(1);
         retries++;
      }
      else {
         return 0;
      }
   } while (retries <= 30);

   pf_errno = PF_PWDFILE_LOCKED;
   return -1;
}

bool_t ws_update_pf_file(struct pf_entry * pfe, int mode)
{
   int i;
   int newfile;
   char usern[12];
   char buffer[255];
   bool_t result = FALSE;
   struct pf_entry user;
   mode_t perms = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;

   memcpy(&user, pfe, sizeof(user));
   if ((newfile = creat(PF_NEWFILE, perms)) != -1) {
      ws_setpfent();
      sprintf(usern, "%s:", pfe->pf_name);

      while (!feof(fp)) {
         if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strncmp(usern, buffer, strlen(usern)) == 0) {
               if (mode & PF_UPDUSER) {
                  sprintf(buffer, "%s:%s:%s:%c:%s:%s:%d:%c\n", user.pf_name,
                     user.pf_passwd, user.pf_start, user.pf_langcode,
                     user.pf_lastaccess, user.pf_lastchange, user.pf_attempts,
                     user.pf_sflag);
                  if (write(newfile, buffer, strlen(buffer)) == -1) {
                     result = FALSE;
                  }
                  else {
                     result = TRUE;
                  }
               }
               else {
                  result = TRUE;
               }
            }
            else {
               if (write(newfile, buffer, strlen(buffer)) == -1) {
                  result = FALSE;
               }
            }
         }
      }

      if (mode & PF_ADDUSER) {
         sprintf(buffer, "%s:%s:%s:%c:%s:%s:%d:%c\n", user.pf_name,
            user.pf_passwd, user.pf_start, user.pf_langcode,
            user.pf_lastaccess, user.pf_lastchange, user.pf_attempts,
            user.pf_sflag);
         if (write(newfile, buffer, strlen(buffer)) == -1) {
            result = FALSE;
         }
         else {
            result = TRUE;
         }
      }

      close(newfile);

      if (result == TRUE) {
         fclose(fp);
         chmod(PF_NEWFILE, perms);
         chown(PF_NEWFILE, (uid_t)0, ws_groupid());
         rename(PF_NEWFILE, PF_FILENAME);
         fp = fopen(PF_FILENAME, "r");
      }
      else {
         remove(PF_NEWFILE);
      }
   }

   return result;
}

int ws_pfunlock(void)
{
   unlink(PF_LOCKFILE);
}

gid_t ws_groupid(void)
{
   struct group * gr;
   char * group_name = getenv("WESNET_GROUP");

   if ((group_name == NULL) || (*group_name == 0)) {
      group_name = "wescom";
   }

   gr = getgrnam(group_name);
   return (gr == NULL) ? (gid_t)200 : gr->gr_gid;
}
