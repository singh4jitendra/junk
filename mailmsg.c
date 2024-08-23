#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

int mailmessage(char * lpRecipient, char *lpFormat, ...)
{
	size_t nSize;
   FILE *fpPipe;
   va_list vaArgList;
   char szCommand[257];
	const char * lpMail = "/bin/mail";

	nSize = sizeof(szCommand) - strlen(lpMail);
	if (strlen(lpRecipient) > nSize)
		return -1;  /* buffer overrun - bail */

	if ((strchr(lpRecipient, ';') != (char *)NULL) ||
		 (strchr(lpRecipient, '|') != (char *)NULL))
		return -1;  /* semi-colons and pipes are not allowed */

   sprintf(szCommand, "mail %s", lpRecipient);

   if ((fpPipe = popen(szCommand, "w")) != (FILE *)NULL) {
      va_start(vaArgList, lpFormat);
      vfprintf(fpPipe, lpFormat, vaArgList);
      va_end(vaArgList);
      pclose(fpPipe);
      return 0;
   }
   else
      return -1;
}
