#ident "@(#)  appsecure.c   Version 1.2   Date Retrieved  8/13/92"
#include <string.h>
#include <stdio.h>

main(argc,argv)
int argc;
char *argv[];
{
extern void Z990A();
char sfunc[2],suser[9],sstat[2];
int i;
sfunc[1] = 0;
sstat[1] = 0;
strcpy(suser,"        ");
sstat[0] = '0';
switch(argc) {
   case 2:   sfunc[0] = argv[1][0];
             if (sfunc[0] != 'J') exit(1);
             break;

   case 3:   sfunc[0] = argv[1][0];
             switch(sfunc[0]) {
               case 'A':case 'C': case 'D': strncpy(suser,argv[2],8);
                    for (i=strlen(suser);i<9;i++) suser[i] = ' ';
                    break;
               default:  exit(1);
             }
             break;
   default:
             exit(1);
}
Z990A(sfunc,suser,sstat);
exit(sstat[0] - 48);
}
