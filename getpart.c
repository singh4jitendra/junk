/* this routing gets a pointer to a string that has the value of the TTY */
/* number defined in the ENVIRONMENT as TTY=NN , converting a to 98 and */
/*  b 99, then copies the first 2 bytes of the string to the pointer passed */
/* from the calling program. Nothing is copied if TTY is undefined in the */
/*  ENVIRONMENT.                                                          */
#include <stdio.h>
#include <string.h>

int GETPART(part_no)
char *part_no;

{
char *_ID = "@(#)getpart.c      1.4";
char *getenv(), *strncpy();
char *ptr;
unsigned sleep();

        if((ptr=(getenv("TTY"))) != NULL){
                if( ptr[0] == 'a' ){
                        (void)strncpy(part_no,"98",2);
                }
                else if( ptr[0] == 'b' ){
                        (void)strncpy(part_no,"99",2);
                }
                else {
                        (void)strncpy(part_no,ptr,2);
                }
        }
        else {
                (void)fprintf(stderr,"Error: TTY not found in environment \n");
                (void)sleep(1);
        }
return(0);
}
