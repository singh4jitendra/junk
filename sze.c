/*****************************************************************/
/* sze.c  Release: 1.3 Last Delta : 12/14/87 @ 15:37:00         */
/* Extracted from SCCS 8/13/92 @ 12:02:26                     */
/* Written by R. Weinschenker NCR 4604                           */
/*****************************************************************/
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#define IBS 1024

FILE *IN, *OUT;
char ibuf[IBS] ;
int rcount, wcount, n, cnt, size;

main(argc,argv)
int argc;
char *argv[];

{

  if (argc < 2) {
	return(usage(argv[0]));
  }
  size=atoi(argv[1]);
  if ((IN=(FILE *)fdopen(0,"r")) == NULL) {
	fprintf(stderr,"FATAL ERROR: OPEN error on stdin \n");
	return(error(argv[0]));
  }
  if((OUT=(FILE *)fdopen(1,"w")) == NULL) {
	fprintf(stderr,"FATAL ERROR: OPEN error on stdout \n");
	return(error(argv[0]));
  }
  for (n=1 ; size > IBS ; n++) {
	if((rcount=fread(ibuf,(sizeof(*ibuf)),IBS,IN)) != IBS) {
    		fprintf(stderr,"Partial record in: %d bytes\n",rcount);
		return(error(argv[0]));
	}
	else {
 		if((wcount=fwrite(ibuf,(sizeof(*ibuf)),rcount,OUT)) != rcount) {
    		    fprintf(stderr,"Partial record out: %d bytes\n",wcount);
		    return(error(argv[0]));
	    	}
	}
	size=(size-IBS);
  }
  if((rcount=fread(ibuf,(sizeof(*ibuf)),size,IN)) != size) {
 	fprintf(stderr,"Partial last record in: %d bytes\n",rcount);
	return(error(argv[0]));
  }
  else {
   	if((wcount=fwrite(ibuf,(sizeof(*ibuf)),rcount,OUT)) != rcount) {
    		fprintf(stderr,"Partial last record out: %d bytes\n",wcount);
		return(error(argv[0]));
   	}
  }
  cnt=(size-wcount);
  fclose(IN);
  fclose(OUT);
  return(cnt);
}
error(name)
char *name;
{
	fprintf(stderr, "%s: Fatal error!\n",name);
	return(-2);
}

usage(name)
char *name;
{
   fprintf(stderr,"Usage: %s size \n",name);
   return(-1);
}

