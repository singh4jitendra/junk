#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "common.h"

socket_t rsvport(unsigned short *port)
{
   socket_t s;
   bool_t done = FALSE;
   struct sockaddr_in sa;

   sa.sin_family = AF_INET;
   sa.sin_addr.s_addr = INADDR_ANY;

   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return -1;

   *port = 20000;

   while (!done) {
      sa.sin_port = *port;
      if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0)
         if (errno == EADDRINUSE) {
            if ((*port += 1) >= 30000)
               return -1;

            continue;
         }
         else
            return -1;

      done = TRUE;
   }

   return s;
}
