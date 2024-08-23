#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

main(argc,argv)
int argc;
char *argv[];
{
void extern SPLTTRANS();
char bnum[7];
int i;
if (argc < 2) {
   strcpy(bnum,"000600");
   SPLTTRANS(bnum);
   exit(0);  
}
i = strlen(argv[1]);
if (i < 6) {
   strcpy(bnum,"000000");
   strcpy(bnum+(6-i),argv[1]);
}
else strncpy(bnum,argv[1],6);
SPLTTRANS(bnum);
exit(0);
}
