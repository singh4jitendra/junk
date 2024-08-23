#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

main()
{
    FILE *t;
    struct stat f;

    t = fopen("test.log", "wt");

    stat("/home2/mikeb/src/06.04.00/genservr.c", &f);
    fprintf(t, "The file mode is %o.\n", f.st_mode);
    fflush(t);
    fclose(t);
}
