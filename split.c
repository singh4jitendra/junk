#include <stdio.h>
#include <string.h>


#define THEBLOCKSIZE 1100000
#define BASENAME "/usr/spool/lp/temp/SPLIT"

main()
{
FILE *infp,*outfp;
char file_name[51];
char ofile_name[51];
unsigned char buffer[THEBLOCKSIZE];
long numread,numwrite;
unsigned char numdisks=0;
char syscommand[81];
unsigned char i;

printf("\b\b\b\n\n\n**********\n");
printf("Enter Input File Name: ");
fflush(stdin);
strcpy(file_name,"/usr/spool/lp/temp/");
gets(file_name+19);
if (!(infp = fopen(file_name,"r+b")))
	{

	printf("Could Not Open Input File - Press Any Key\n");
	fflush(stdin);
	getchar();
	exit(1);
	}
numread=THEBLOCKSIZE;

while (numread != 0 && !feof(infp))
	{
	numread = fread(buffer,1,THEBLOCKSIZE,infp);
	if (numread!=0)
		{
		strcpy(ofile_name,BASENAME);
		sprintf(ofile_name+strlen(ofile_name),".%d",++numdisks);
		if (!(outfp=fopen(ofile_name,"w+b")))
			{
		    printf("Could Not Create Output File - Press Any Key\n");
                    fflush(stdin);
                    getchar(); 
			exit(1);
			}
		else
			printf("Created %s\n",ofile_name);


		numwrite=fwrite(buffer,1,numread,outfp);
		if (numwrite != numread)
			{
			printf("Write Failed - Press Any Key\n");
                        fflush(stdin);
                        getchar();
			exit(1);
			}

		fclose(outfp);




		}
	}

fclose(infp);

for (i=1;i<=numdisks;i++)

	{
	printf("Insert Disk %d And Press Enter\n",i);
	fflush(stdin);
	getchar();

	printf("Please Wait ...\n");
	strcpy(ofile_name,BASENAME);
	sprintf(ofile_name+strlen(ofile_name),".%d",i);
	sprintf(syscommand,"doscp %s a:",ofile_name);
	system(syscommand);
	remove(ofile_name);
	}

}
