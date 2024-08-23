#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
*******************************************************************************
* Interface between AcuCobol and the Linux command line
*******************************************************************************
* ISSUE   WHO  DATE      DESCRIPTION
* ------  ---  --------  ------------------------------------------------------
*         TM   10/18/12  Created the program
* 7490    MAM  3/6/13    Use pid instead of ppid
*******************************************************************************
*/

int call_shell(char *p_sInScript, char *p_sOutFile);

int main()
{
   char InScript[] = "atest";
   char OutFile[16];

   call_shell(InScript, OutFile);
   return 0;
}

/*
*******************************************************************************
*  Function:    call_shell
*
*  Description: This function allows an AcuCobol program calling a shell script
*               to receive the output of the shell script.  By default the
*               only thing that can be passed back to AcuCobol is the return
*               code.
*
*               The function picks the name of a temporary file that is
*               guaranteed to be unique to this run.  In this case it uses the
*               parent's process ID (the process ID of the calling AcuCobol
*               program).  That file name is passed down to the shell script
*               through the command line and up to the Cobol program through
*               linkage.
*
*  Parameters:  sInScript   The command line to run.  This is limited to 1000
*                           characters and can include parameters.  The
*                           parameter TMPFILE=_UNIQUE_FILE_ is added by this
*                           function to the end of the command line call.  The
*                           program or shell script called must write all
*                           return information to that file.
*
*               sOutFile    The name of the file (_UNIQUE_FILE_ in the
*                           descripton of sInScript above) where the output
*                           will be written.  This parameter is 16 characters
*                           and will be passed back through linkage to the
*                           AcuCobol program.
*
*  Returns:     The exit status of the called script.
*******************************************************************************
*/
int call_shell(char *p_sInScript, char *p_sOutFile)
{
   pid_t ppid;

   char sOutFile[16];
   char sInScript[1000];

   sOutFile[0] = '\0';

   ppid = getpid();
   if (ppid < 0) return 2;

   snprintf(sOutFile, 16, "/tmp/%06d.tmp", ppid);
   sprintf(sInScript, "%s TMPFILE=%s", p_sInScript, sOutFile);

   strcpy(p_sOutFile, sOutFile);

   return system(sInScript);
}
