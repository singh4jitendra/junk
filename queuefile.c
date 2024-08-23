#if defined(_HP_TARGET)
#   define _INCLUDE_POSIX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _INCLUDE_HPUX_SOURCE
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>

char *getqueuefilename(char *name, char *prefix)
{
    struct timeval curtime;

    gettimeofday(&curtime, 0);

    sprintf(name, "%s.%lu.%lu", prefix, curtime.tv_sec, curtime.tv_usec);

    return name;
}
