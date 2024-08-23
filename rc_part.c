#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "sub.h"

int ws_getpartition(char * name, int num_args, Argument args[], int initial)
{
   char * dir2;
   int ttyLength;
   char * suffix;
   int dir2Length;
   char * ttyNumber;
   int suffixLength;

   if (num_args != 3) {
      return Halt;
   }

   ttyLength = a_size(args[0]);
   ttyNumber = (char *)calloc(ttyLength + 1, sizeof(char));
   if (ttyNumber == NULL) {
      return Halt;
   }

   suffixLength = a_size(args[1]);
   suffix = (char *)calloc(suffixLength + 1, sizeof(char));
   if (suffix == NULL) {
      return Halt;
   }

   dir2Length = a_size(args[2]);
   dir2 = (char *)calloc(dir2Length + 1, sizeof(char));
   if (dir2 == NULL) {
      return Halt;
   }

   CobolToC(ttyNumber, args[0].a_address, ttyLength + 1, ttyLength);
   CobolToC(suffix, args[1].a_address, suffixLength + 1, suffixLength);
   CobolToC(dir2, args[2].a_address, dir2Length + 1, dir2Length);

   CreateUserPartition(ttyNumber, suffix, dir2);

   CtoCobol(args[0].a_address, ttyNumber, ttyLength);
   CtoCobol(args[1].a_address, suffix, suffixLength);
   CtoCobol(args[2].a_address, dir2, dir2Length);

   free(ttyNumber);
   free(suffix);
   free(dir2);

   return Okay;
}

int ws_delpartition(char * name, int num_args, Argument args[], int initial)
{
   int len;
   char * dir2;

   if (num_args != 1) {
      return Halt;
   }

   len = a_size(args[0]);
   dir2 = (char *)calloc(len + 1, sizeof(char));
   if (dir2 == NULL) {
      return Halt;
   }

   CobolToC(dir2, args[0].a_address, len + 1, len);
   wzd_delete_partition(dir2);
   free(dir2);

   return Okay;
}
