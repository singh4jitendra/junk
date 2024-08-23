#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 ******************************************************************************
 * 12/08/05 mjb 05078 - Changed the call from getlogin to cuserid to obtain
 *                      the username of the effective user id.
 ******************************************************************************
 */
WHOM(name,code)     
char name[12], code[1];
{
char *tmp;
char *_ID = "@(#)whom.c	1.1";

	if ((tmp=cuserid(NULL)) != NULL){
		if((strncpy(name,tmp,12)) != NULL){
			code[0]='0';
			return(0);
			}	
	}	
	code[0]='1';
	return(0);
}
