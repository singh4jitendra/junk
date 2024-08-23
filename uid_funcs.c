#include <sys/types.h>
#include <stdlib.h>
#include "sub.h"
#include "common.h"
#include <string.h>

char * ws_get_user_by_uid(pid_t, char *, int);
uid_t ws_get_process_owner(pid_t);

int ws_getuid(char * name, int num_args, Argument args[], int initial)
{

   int pid_str_length;
   char * pid_str;
   pid_t pid;

   int usrName_length;
   char * usrName;

   if (num_args != 2) {
      return Halt;
   }

   pid_str_length = a_size(args[0]) + 1;
   pid_str = (char *)calloc(pid_str_length + 1, sizeof(char));
   if (pid_str == NULL)
   	{
		return Halt;
	}

   usrName_length = a_size(args[1]) + 1;
   usrName = (char *)calloc(usrName_length + 1, sizeof(char));
   if (usrName == NULL)
    {
   		return Halt;
	}


   CobolToC(pid_str, args[0].a_address, pid_str_length + 1, pid_str_length);
   //CobolToC(usrName, args[1].a_address, usrName_length + 1, usrName_length);
   memset(usrName, 0, usrName_length+1);
   pid = atol(pid_str);


   if (ws_get_user_by_uid(ws_get_process_owner(pid), usrName, usrName_length) == NULL)
   	{
		return Halt;
	}


   //ws_get_user_by_uid(ws_get_process_owner(pid_t), usrName, usrName_length);

   CtoCobol(args[0].a_address, pid_str, pid_str_length);
   CtoCobol(args[1].a_address, usrName, usrName_length);

   free(usrName);
   free(pid_str);

   return Okay;
}
