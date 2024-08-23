#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char * argv[])
{
	char * null;
	char buffer[256];
	FILE * old = fopen("XMMPJ", "r");
	FILE * new = fopen("XMMPJ.tmp", "w+");

	if (old == NULL || new == NULL) return 1;

	while (!feof(old)) {
		fgets(buffer, sizeof(buffer), old);
		fprintf(new, "%s\n", buffer);
	}

	fclose(old);
	fclose(new);

	if (remove("XMMPJ") != 0) return 2;
	if (rename("XMMPJ.tmp", "XMMPJ") != 0) return 3;

	return 0;
}
