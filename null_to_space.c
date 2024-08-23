#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

char ifile[80], ofile[80];
#ifndef _GNU_SOURCE
extern char* sys_errlist[], *optarg;
extern int errno, optind, opterr, optopt;
int getopt();
#endif

main(argc,argv)
int argc;
char **argv;
{
   int ifd=0, ofd=1, count;
   char cmdarg, buffer[4096];
   char *c;


   while ((cmdarg=(char)getopt(argc,argv,"i:o:")) != (char)EOF) 
      switch(cmdarg) {
	case 'i':
		ifd=-1;
		sprintf(ifile,"%s",optarg);
		break;
	case 'o':
		ofd=-1;
		sprintf(ofile,"%s",optarg);
		break;
 	case '?':
		fprintf(stderr,"Usage: %s -i input_file -o output_file\n",argv[0]);
		exit(2);
      }

   if ((ifd != 0) && ((ifd=open(ifile, O_RDONLY | O_NDELAY )) < 0)) {
	fprintf(stderr,"ERROR: %d - unable to open %s\n",errno,ifile);
#ifdef _GNU_SOURCE
	fprintf(stderr,"%s\n",strerror(errno));
#else
	fprintf(stderr,"%s\n",*(sys_errlist+errno));
#endif
	exit(2);
   }

   if ((ofd != 1) && ((ofd=creat(ofile,(mode_t)0644)) < 0)) {
	fprintf(stderr,"ERROR: %d - unable to open %s\n",errno,ofile);
#ifdef _GNU_SOURCE
	fprintf(stderr,"%s\n",strerror(errno));
#else
	fprintf(stderr,"%s\n",*(sys_errlist+errno));
#endif
	exit(2);
   }

   while ((count=read(ifd,buffer,(unsigned)4096)) > 0) {
      c=buffer;
      while ((c - buffer) < count) {
	if (*c == '\0') *c=' ';
	c++;
      }
      write(ofd,buffer,count);
   }


   if (ofd) close(ofd);
   if (ifd) close(ifd);
   exit(0);
}
