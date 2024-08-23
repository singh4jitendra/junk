/*      To compile for 850:
             cc -o qfix qfix.c
        To compile for 650:
             cc.10 -o qfix qfix.c
*/
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>


main(argc,argv)
int argc;
char *argv[];
{

int qmgrid;
int mypid;
struct msqid_ds msqbuf;
if (argc < 2) exit(0);
qmgrid = atoi(argv[1]);
msgctl(qmgrid,IPC_RMID,&msqbuf);
return(0);
}
