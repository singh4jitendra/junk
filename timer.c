#include <stdio.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/param.h>
#include <string.h>

#define TICKS HZ

typedef struct tms tms;

static long time_start, time_stop;
static tms tms_start, tms_stop;
static double start, stop, seconds;

long StartTimer(void)
{
    if ((time_start = times(&tms_start)) == -1)
        perror("volumetest");

    return time_start;
}

long StopTimer(void)
{
    if ((time_stop = times(&tms_stop)) == -1)
        perror("volumetest");

    return time_stop;
}

double ElapsedRealTime(void)
{
    seconds = (double)(time_stop - time_start) / (double)TICKS;

    return seconds;
}
