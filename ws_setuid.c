#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include "sub.h"
#include "common.h"

int ws_setuid(char * name, int num_args, Argument args[], int initial)
{
   int len;
   uid_t uid;
   char * username;
   struct passwd * pwent;

   if (num_args != 1) {
      return Halt;
   }

   if (args[0].a_type == Alphanum) {
      len = args[0].a_length;
      username = (char *)calloc(len + 1, sizeof(char));
      if (username != NULL) {
         CobolToC(username, args[0].a_address, len + 1, len);
         pwent = getpwnam(username);
         if (pwent != NULL) {
            uid = pwent->pw_uid;
            return_code = (long)setresuid(uid, uid, uid);
            if (return_code == 0) {
               setenv("LOGNAME", username, 1);
            }

            free(username);
         }
         else {
            return_code = 1;
         }

         return Okay;
      }
   }

   return Halt;
}
