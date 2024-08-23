#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "ws_pwfile.h"
#include "sub.h"

int ws_login(char * name, int num_args, Argument args[], int initial);
int ws_change_passwd(char * name, int num_args, Argument args[], int initial);
char * get_alnum_arg(Argument arg);

int ws_login(char * name, int num_args, Argument args[], int initial)
{
   int len;
   int * status;
   char * username;
   char * pwd;

   if (num_args != 3) {
      return Halt;
   }

   /* convert the user name */
   if ((username = get_alnum_arg(args[0])) == NULL) {
      return Halt;
   }

   /* convert the password */
   if ((pwd = get_alnum_arg(args[1])) == NULL) {
      free(username);
      return Halt;
   }

   status = (int *)args[2].a_address;
   *status = 0;

   if (ws_pfopen() == 0) {
      if (ws_authenticate_login(username, pwd) == FALSE) {
         *status = (short)ws_getlastpferror();
      }

     ws_pfclose();
   }
   else {
      *status = (short)ws_getlastpferror();
   }

   free(username);
   free(pwd);

   return Okay;
}

int ws_change_passwd(char * name, int num_args, Argument args[], int initial)
{
   int * status;
   char * username;
   char * oldpwd;
   char * newpwd;

   if (num_args != 4) {
      return Halt;
   }

   /* convert the user name */
   if ((username = get_alnum_arg(args[0])) == NULL) {
      return Halt;
   }

   /* convert the old password */
   if ((oldpwd = get_alnum_arg(args[1])) == NULL) {
      free(username);
      return Halt;
   }

   /* convert the new password */
   if ((newpwd = get_alnum_arg(args[2])) == NULL) {
      free(newpwd);
      return Halt;
   }

   status = (int *)args[3].a_address;
   *status = 0;

   if (ws_pfopen() == 0) {
      if (ws_chg_pwd(username, oldpwd, newpwd) == FALSE) {
         *status = (short)ws_getlastpferror();
      }

      ws_pfclose();
   }
   else {
      *status = (short)ws_getlastpferror();
   }

   free(username);
   free(oldpwd);
   free(newpwd);

   return Okay;
}

char * get_alnum_arg(Argument arg)
{
   int len;
   char * buffer = NULL;

   if (arg.a_type == Alphanum) {
      len = arg.a_length;
      buffer = (char *)calloc(len + 1, sizeof(char));
      if (buffer != NULL) {
         CobolToC(buffer, arg.a_address, len + 1, len);
      }
   }

   return buffer;
}
