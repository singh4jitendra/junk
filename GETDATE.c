/*  GETDATE.c  --  This program can be called by a cobol program as      */
/*                 follows:  CALL "GETDATE" USING W-DATE8.  After        */
/*                 this program has been executed, W-DATE8 will contain  */
/*                 the current date in the format YYYYMMDD.              */

#include <time.h>
#include <string.h>

struct outrec {
    char year[4];
    char month[2];
    char day[2];
};

void ltofa(num, buff, len)
int num;
char *buff;
int len;
{
    buff[len] = 0;
    while (len--)  {
	buff[len] = (char) ((num % 10) + 48);
	num /= 10;
    }
}

int GETDATE(in)
struct outrec *in;
{
    time_t stim;
    struct tm *tms;
    char *tstr;
    char buff[5];
    int i;

    stim = time((long*) 0);
    tms = localtime(&stim);
    tms->tm_year += 1900;
    ltofa(tms->tm_year, buff, 4);
    for (i=0; i<4; i++)
	in->year[i] = buff[i];
    ltofa(tms->tm_mday, buff, 2);
    for (i=0; i<2; i++)
       in->day[i] = buff[i];
    tms->tm_mon++;
    ltofa(tms->tm_mon, buff, 2);
    for (i=0; i<2; i++)
       in->month[i] = buff[i];
    return(0);
}
