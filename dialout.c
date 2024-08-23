/*
 * Project Number: 50046
 * Module: dialout.c
 * Last Modified: 02/19/97
 *
 * Change added to allow baud rates up to 38400 (acuH*)
*/

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <signal.h>
#include <termio.h>
#include <fcntl.h>
#include <errno.h>
#define MAXBUF 800
#define MAXWORD 40
#define MAXBAUD 2
#define MAXSCNT 7
#define BUF 80
#define TRUE  1
#define FALSE 0
#define ERROR -1
#define READ 1
#define WRITE -1
#define WAIT  256
#define STTY  257
#define EOT   258
#define NL    259
#define SNULL 260
#define OTHER 261
#define CR    262
#define LF    263
#define BREAK 264
#define TOUT  20

#ifndef CNUL
#define CNUL 0
#endif

int d_cnt, d_gfp1,d_gfp2,d_gfp3, d_debug, d_dcap, d_NSC, d_TIME, d_tout;
int d_slength, d_stder, d_ECHO, num_modem, speed;
char d_param[60][40], d_modem[50], d_Lfile[32], *max_modem; 
struct termio d_oldtty, d_newtty ,d_out_pt;
extern unsigned alarm(), sleep();

#ifndef _GNU_SOURCE
extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;
#endif

FILE *d_file, *m_file;

static char d_VERSION[]="@(#)dialout.c 3.00";


DIALOUT(path,line,mode,timeout,stat)
char path[], line[], mode[], timeout[], stat[];

{
int dstat, i, act, wc, action = WRITE, quit();
char device[50], *getenv(), command[40];

	signal(SIGALRM,(void(*)(int))quit);
	signal(SIGINT,(void(*)(int))quit);
	d_modem[0]='\0'; 
	d_TIME=TRUE;
	d_debug = FALSE;
	d_dcap = FALSE;
	d_NSC=FALSE;
        d_ECHO=FALSE;
	if ((mode[0] == 'D') || ((getenv("DBUG")) != NULL)){
		d_debug=TRUE;
	}
	if ((mode[0] == 'C') || ((getenv("DCAP")) != NULL)){
		d_dcap=TRUE;
	}
        if (line[0] == 'N')
            d_NSC = TRUE;
        if (line[0] == 'E')
            d_ECHO = TRUE;

	/* Open file to record debug messages, if in debug mode */
	if (d_debug){
		if((d_file=((FILE *)fopen("/tmp/d_debug","w+"))) == NULL) {
			fprintf(stderr,"Open of debug file failed\n");
			fprintf(stderr,"Standard error will be used!\n");
			d_file=((FILE *)fdopen(2,"w"));
			d_stder=TRUE;
		}
		else{
			fprintf(stderr,"Debug data output to file \"/tmp/d_debug\" \n");
			d_stder=FALSE;
		}
		fflush(stderr);
	}

	/* verify the timeout value, if invalid, set to default value */
	if ((d_tout=(atoi(timeout))) == 0){
		d_tout=TOUT;
		sprintf(timeout,"%d",d_tout);
		if (d_debug)
		  fprintf(d_file,"Timeout value invalid - %d used \n",d_tout);
	}

	if (d_debug){
		fprintf(d_file,"path: { %s }\n",path);
		fprintf(d_file,"line option: %s\n",line);
		fprintf(d_file,"mode: %s\n",mode);
		fprintf(d_file,"timeout value: %s\n",timeout);
	}

	/* check information in the path field */
	parce(path); 
	if (d_cnt < 1 ){
		if (d_debug){
			fprintf(d_file,"No phone number! \n");
		}
		stat[0]='7';   
		stat[1]='\0';   
		return(1);
	}
	if (d_debug) {
		fprintf(d_file,"%d characters in dial string\n",d_slength );
		fprintf(d_file,"%d words in dial string\n",d_cnt );
		fprintf(d_file,"phone number:[ %s ]{1}\n",d_param[0]);
		for(i=1; i < d_cnt; i++){
		  wc=(i + 1);
		  fprintf(d_file,"Send => [ %s ]{%d}  ",d_param[i],wc++);
	 	  if ( (++i) < d_cnt){
		   fprintf(d_file,": Sense <= [ %s ]{%d}\n",d_param[i],wc);
		  }
		  else{
			fprintf(d_file,"\n");
		       }
		}
	}
	if ( !( (d_param[0][0] == 'T')||(d_param[0][0] == 'P') ) ){
		if (d_debug){
		  fprintf(d_file,"Invaild dial TYPE: %c !\n",d_param[0][0]);
		}
		stat[0]='7';   
		stat[1]='\0';   
		return(1);
	}
	stat[0]='I';

	/* set stty values */
	if (ioctl(0,TCGETA,&d_oldtty)<0){
		if (d_debug){
		  fprintf(d_file,"IOCTL error getting structure for stdin.\n");
		}
		stat[0]='9';   
		stat[1]='\0';   
		return(1);
	}
	if (d_debug){
		fprintf(d_file,"stty values for stdin saved: \n");
		fprintf(d_file,"c-flags : %o\n" ,d_oldtty.c_cflag);
		fprintf(d_file,"i-flags : %o\n" ,d_oldtty.c_iflag);
		fprintf(d_file,"o-flags : %o\n" ,d_oldtty.c_oflag);
		fprintf(d_file,"l-flags : %o\n" ,d_oldtty.c_lflag);
		fprintf(d_file,"INTR : %u \n",d_oldtty.c_cc[0]);
		fprintf(d_file,"QUIT : %u \n",d_oldtty.c_cc[1]);
		fprintf(d_file,"ERASE : %u \n",d_oldtty.c_cc[2]);
		fprintf(d_file,"KILL : %u \n",d_oldtty.c_cc[3]);
		fprintf(d_file,"EOF : %u \n",d_oldtty.c_cc[4]);
		fprintf(d_file,"EOL : %u \n",d_oldtty.c_cc[5]);
	}

	d_newtty.c_iflag = (BRKINT+IGNPAR+ISTRIP+ICRNL+IXON);
	d_newtty.c_oflag = (OPOST+ONLCR+TAB3);
	d_newtty.c_cflag = d_oldtty.c_cflag;
	d_newtty.c_lflag = (ISIG+ICANON+ECHO+ECHOK); 
	d_newtty.c_cc[0] = CINTR;
	d_newtty.c_cc[1] = CQUIT;
	d_newtty.c_cc[2] = CERASE;
	d_newtty.c_cc[3] = CKILL;
	d_newtty.c_cc[4] = CEOF; 
	d_newtty.c_cc[5] = CNUL; 
	
	if (ioctl(0,TCSETA,&d_newtty)<0){
		if (d_debug){
		  fprintf(d_file,"error setting  new stty values for stdin.\n");
		}
		stat[0]='9';   
		stat[1]='\0';   
		restore();
		return(1);
	}
	if (d_debug){
		fprintf(d_file," new stty values set for stdin\n");
	}
	stat[0]='S';


      /* determine number of modems */
      if ((max_modem = getenv("NMODEM")) != NULL)
            num_modem = atoi(max_modem);
      else
        if ((m_file = ((FILE *)fopen("numodem", "r"))) == NULL)
               num_modem = 2;
        else  {
               if (fscanf(m_file, "%d", &num_modem) == 0)
                   num_modem = 2;
               fclose(m_file);
           }

      if  (d_debug)
            fprintf(d_file, " Using %d modems\n", num_modem);

     /* call getdevice()- determines which device in /dev to use */
      i = getdevice(device);
      if (i)  {
           stat[0] = '1';
           stat[1] = '\0';
           restore();
           return(1);
        } 
        
      if (d_debug){
	     fprintf(d_file,"USING %s \n",device);
           fflush(d_file);
	  }

	/* check that the device is not locked, then lock it */
	if( ( d_gfp2=(cu_lock(device)) ) > 0 ){
		alarm(5);
		stat[0]='O';
		/* open the modem for reading and writing */
		while(((d_gfp1=open(device,O_RDWR)) != -1) && (d_TIME)){
			if (d_debug){
				fprintf(d_file,"opened  %s \n",device);
                                fflush(d_file);
			}
			alarm(0);
			stat[0]='D';
			/* set the speed to be used by the modem */
			setmodem(speed);
			if(d_debug){
				fprintf(d_file,"initialized %s \n",device);
                                                fflush(d_file);
			  }

			/* dialout from the modem */
			if ((dstat=dialup(d_param[0],timeout)) == ERROR){
				fprintf(stdout,"DIALING ERROR! \n");
				stat[0]='6';
				stat[1]='\0';
				restore();
				hangup();
				return(1);
			}
			if (dstat == FALSE){
				fprintf(stdout,"CALL to remote FAILED! \n");
				stat[0]='2';
				stat[1]='\0';
				restore();
				hangup();
				return(1);
			}
			stat[0]='L';
			fprintf(stdout,"CALL Successful  \n");

			/* login to the remote location */
			fprintf(stdout,"AUTO login in progress...\r");
			fflush(stdout);
			for(i = 1; i < d_cnt; i++) {
				/* convert keyword to data    */
				act = whatis(d_param[i]);
				switch (act){
				  case WAIT:
				       sleep((unsigned)(atoi(d_param[++i])));
				       break;
				  case EOT:
				       d_param[i][0]='\4';
				       d_param[i][1]='\0';
				       break;
				  case NL:
				       d_param[i][0]='\15';
				       d_param[i][1]='\12';
				       d_param[i][2]='\0';
				       break;
				  case CR:
				       d_param[i][0]='\15';
				       d_param[i][1]='\0';
				       break;
				  case LF:
				       d_param[i][0]='\12';
				       d_param[i][1]='\0';
				       break;
 				  default:
				       break;
				}  
				if ((act != WAIT) && (act != SNULL)){
				  if (action == READ){
				    if (d_debug ){
				      switch (act){
					case EOT:
				 	     fprintf(d_file,"\t\t\t\tWaiting on :=> EOT\n"); 
				             break;
					case NL:
				             fprintf(d_file,"\t\t\t\tWaiting on :=> NL\n"); 
					     break;
					case CR:
					     fprintf(d_file,"\t\t\t\tWaiting on :=> CR\n"); 
					     break;
					case LF:
					     fprintf(d_file,"\t\t\t\tWaiting on :=> LF\n"); 
					     break;
					default:
					     fprintf(d_file,"\t\t\t\tWaiting on :=> %s\n",d_param[i]);
					     break;
				       }
			           }
			     	  if (getword(d_gfp1,d_param[i])){
				    alarm(0);
				  }
				  else {
				    fprintf(stdout,"LOGIN failed waiting for: %s [%d]\n",d_param[i],i);
			            fflush(stdout);
				    sleep(3);
				    restore();
				    hangup();
				    stat[0]='3';
				    stat[1]='\0';
				    return(1);
				  }
				}
				else{
				  switch (act){
					case EOT:
				             if (d_debug){
						fprintf(d_file,"\t\t\t\tSending :=> EOT\n"); 
					     }
					     write(d_gfp1,d_param[i],(unsigned)(strlen(d_param[i])));
					     break;
					case NL:
					     if (d_debug){
						fprintf(d_file,"\t\t\t\tSending :=> NL\n"); 
					     }
					     write(d_gfp1,d_param[i],(unsigned)(strlen(d_param[i])));
					     break;
				        case CR:
				    	     if (d_debug){
						fprintf(d_file,"\t\t\t\tSending :=> CR\n"); 
					     }
					     write(d_gfp1,d_param[i],(unsigned)(strlen(d_param[i])));
					     break;
					case LF:
					     if (d_debug){
						fprintf(d_file,"\t\t\t\tSending :=> LF\n"); 
					     }
					     write(d_gfp1,d_param[i],(unsigned)(strlen(d_param[i])));
					     break;
					default:
					     if (d_debug){
						fprintf(d_file,"\t\t\t\tSending :=> %s\n",d_param[i]); 
					     }
					     write(d_gfp1,d_param[i],(unsigned)(strlen(d_param[i])));
					     write(d_gfp1,"\015",1);
					     break;
					}
				     }
				  action = -action;
				}
				if (act == SNULL){
					action= -action;
				}
			}
			if (d_debug){
				fprintf (d_file,"LOGIN successful \n");
                                fflush(d_file);
			}
			stat[0]='C';
			command[0]='\0';
			if (d_NSC) {
				(void)strncpy(command,"cu -e -b7 -h -l ",17);
			}
			else if(d_ECHO) {
				(void)strncpy(command,"cu -e -b7 -h -l ",17);
			}
			else {
				(void)strncpy(command,"cu -e -b7 -l ",14);
			}
			/* perform Call UNIX */
			(void)strcat(command,device);
			if (d_dcap){
				(void)strcat(command," | tee /tmp/dial.out");
			  }
			if (d_debug){
				fprintf(d_file,"system call: %s\n",command);
                                                fflush(d_file);
			  }
			fprintf(stdout,"AUTO LOGIN Successful        \n");
			fprintf(stdout,"Envoking Terminal Emulator - use:  ~.{RETURN}  to quit\n");
			fflush(stdout);
			/* unlock the device */
			d_gfp2=(cu_ulock());
			if ((system(command)) != 0 ){
				if (d_debug){
					fprintf(d_file,"CU session ERROR! \n");
                                        fflush(d_file);
				}	
				restore();
				hangup();
				stat[0]='4';
				stat[1]='\0';
				return(1);
			}
			fprintf(stdout,"REMOTE TERMINAL SESSION COMPLETE \n");
			restore();
			hangup();
			stat[0]='0';
			stat[1]='\0';
			return(0);
		}
		if (d_debug){
			fprintf(d_file,"LINE OPEN ERROR! \n");
                        fflush(d_file);
		}
		stat[0]='8';
		stat[1]='\0';
		restore();
		hangup();
		return(1);
	}
	if (d_debug){
		fprintf(d_file,"ERROR -  CU Lock on %s! \n",device);
                fflush(d_file);
	}
	stat[0]='5';
	stat[1]='\0';
	restore();
	hangup();
	return(1);
}

/**************************SUBPROGRAMS****************************/
dialup(num,timeout)
char *num, *timeout;
{
	int code;
	code=FALSE;
	write(d_gfp1,"ATH",3);
	write(d_gfp1,"\r",1);
	delay();
	if (getword(d_gfp1,"OK")){
		if (d_debug) { 
			fprintf(d_file,"Modem accessed\n");
                        fflush(d_file);
		} 
		write(d_gfp1,"ATS7=",5);
		write(d_gfp1,timeout,(unsigned)(strlen(timeout)));
		write(d_gfp1,"\r",1);
		delay();
		if (getword(d_gfp1,"OK")){
			if (d_debug) { 
				fprintf(d_file,"Timer value %s set\n",timeout);
                                		fflush(d_file);
			  }
			write(d_gfp1,"ATD",3);
			write(d_gfp1,num,(unsigned)(strlen(num)));
			write(d_gfp1,"\r",1);
			fprintf(stdout,"Dialing...\r");
			fflush(stdout);
			delay();
			if (getword(d_gfp1,"CONNECT")){
				code=TRUE;
			}
			else{
				code=FALSE;
			}
		}
		else {
			code=(ERROR);
			if (d_debug) {
				fprintf(d_file,"MODEM COMMAND ERROR\n");
                                   		fflush(d_file);
			  }
		         }
	}
	else{
		code=(ERROR);
		if (d_debug) {
			fprintf(d_file,"ERROR INITIALIZING MODEM\n");
                        fflush(d_file);
		}
	}
	return(code);
}

hangup()
{
	/* remove modem lock file if one is named */
	if ((strlen(d_modem)) > 0){
		unlink(d_modem);
	}
	/* if dialout file is open  close it */
	if (d_gfp1>0){
		if((d_gfp1=(close(d_gfp1))) != 0){
			fprintf(stderr,"Error closing %s \n",d_modem);
		}
	}
	/* if cu lock file assigned  remove it */
	if (d_gfp2>0){
		if ( (cu_ulock()) < 0 ){
			fprintf(stderr,"Error removing lock file!\n");
		}
	}
	if (d_debug){
		fprintf(d_file,"disconnecting %s\n",d_modem);
                fflush(d_file);
	}
	/* if debug file open   close it*/
	if ((d_debug) && (d_stder == FALSE)){
		if(fclose(d_file)){
			fprintf(stderr, "Can't close debug out file \n");
		}
	}
	fflush(stderr);
	return;
}

restore()

{
	fflush(stdout);
	fflush(stderr);
	if (ioctl(0,TCSETA,&d_oldtty)<0){
		fprintf(stderr," error resetting CRT \n");
	}
	else if (d_debug){
		fprintf(d_file," stty values for stdin restored at hangup \n");
	}
}

parce(str1)
char str1[MAXBUF];
{
int i, letter, word, FIRST_CHAR;

	i = 0; /* string index */
	word = 0; /* word counter */
	letter = 0; /* letter of word index */
	d_cnt = 0; /* number of words found in string */
	
	while ((str1[i] != '\0') && (str1[i] != '\n') && ( i < MAXBUF)){
	  while(((str1[i] == ' ') || (str1[i] == '\t')) && ( i < MAXBUF)){
	     i++;
	   } /* skip white space */
	  FIRST_CHAR = TRUE; 
	  while ((str1[i] != ' ') && (str1[i] != '\0') && ( i < MAXBUF ) &&
		(str1[i] != '\n') && (str1[i] != '\t')){
		if (FIRST_CHAR){
	  	  d_cnt++;
		}
		if ((str1[i] == '"') && FIRST_CHAR ){ /* quoted word loop */
		  i++; /* skip beginning quote */
		  while ((str1[i] != '"') && (i < MAXBUF) && (str1[i] != '\0')){
			if (letter < MAXWORD){
				d_param[word][letter] = str1[i];
				d_param[word][++letter] = '\0';
			}
			i++;
		   } 
		  i++; /* skip closing quote */
		} 
		else { 
			if (letter < MAXWORD){
			d_param[word][letter] = str1[i];
			d_param[word][++letter] = '\0';
		}
	  	  i++;
		  FIRST_CHAR = FALSE;
	  	  }
		}
		letter=0;
		d_param[++word][letter] = '\0';
	}
	d_slength=i;

}

setmodem(int baud)
{
	d_out_pt.c_iflag = (IGNBRK|INPCK|ISTRIP|IXON|IXOFF|0040000);
	d_out_pt.c_oflag = (CR1);
	d_out_pt.c_cflag = (baud|CS7|CREAD|PARENB|HUPCL);
	d_out_pt.c_lflag = (0); 
	d_out_pt.c_cc[4] = 1; 
	d_out_pt.c_cc[5] = 1; 
	if (d_gfp1 < 3){
		if (d_debug){
			fprintf(stderr,"Line not open for IOCNTL\n");
		}
		return(1);	
	}
	if (ioctl(d_gfp1,TCSETA,&d_out_pt)<0){
		if (d_debug){
			fprintf(stderr,"I/O control error on %s\n",d_modem);
		}
		return(1);	
	}
	return(0);
}
getword(fp,str)
char *str;
int fp;

{
register int i;
int cc = 0;
int HIT = FALSE;
int rd;
char ch, word[MAXBUF];
int cnt=0;

        i=0;
        word[i]='\0';
        alarm((unsigned)d_tout);
        while( (!HIT) && ((rd=read(fp,&ch,1)) == 1) && (d_TIME) ){
        cnt++;
        fprintf(d_file,"cnt=%d\n",cnt);
        fprintf(d_file,"char=%c\n",ch);
                ch=(ch & 0x7F);
        fprintf(d_file,"converted char=%c\n",ch);
                if(ch <= '\037' || ch >= '\173'){
                        if (d_debug){
                                if (cc == 20){
                                        cc=0;
                                }
                                if (cc == 0) {
                                        fprintf(d_file,"\nCH: ");
                                }
                                fprintf(d_file,"%.2X ",ch);
                                cc++;
                        }
                        i=0;
                        word[i]='\0';
                }
                else{
                        word[i]=ch;
                        word[++i]='\0';
                        if (d_debug){
                                if (cc != 0){
                                        fprintf(d_file,"\n");
				}
                                fprintf(d_file,"WORD = %s \r",word);
                                cc=0;
                        }
                }
                if (substrg(word,str)){
                        HIT=TRUE;
                        alarm(0);
                }
        }
        if (d_debug){
fprintf(d_file,"\n\tWanted{%s} Got{%s} Hit{%d} Time{%d}\n",str,word,HIT,d_TIME);
        }
        if (rd != 1){
                if (d_debug){
                        if ( rd == 0 ){
                                fprintf(d_file,"DSR drop from modem \n");
                        }
                        else{
                                if ( errno <= sys_nerr ){
#ifdef _GNU_SOURCE
		fprintf(d_file,"I/O port read: %s\n",strerror(errno));
#else
                fprintf(d_file,"I/O port read: %s\n",sys_errlist[errno]);
#endif
                                }

                        }
                }
                d_TIME=FALSE;
        }
        return(d_TIME);
}

quit()
{

	d_TIME=FALSE;
	if (d_debug){
		fprintf(d_file,"\nTIMEOUT \n");
	}
	signal(SIGALRM,(void(*)(int))quit);
	return;
}

whatis(arg)
char *arg;
{
	if (strcmp(arg,"WAIT") == 0){
		return(WAIT);
	}
	if (strcmp(arg,"EOT") == 0){
		return(EOT);
	}
	if (strcmp(arg,"NL") == 0){
		return(NL);
	}
	if (strcmp(arg,"CR") == 0){
		return(CR);
	}
	if (strcmp(arg,"LF") == 0){
		return(LF);
	}
	if (strcmp(arg,"NULL") == 0){
		return(SNULL);
	}
	if (strcmp(arg,"BREAK") == 0){
		return(BREAK);
	}
	return(OTHER);
}

substrg(s,t)
char s[], t[];
{
int i, j, k;

        for(i=0; s[i] != '\0'; i++){
                for(j=i,k=0; t[k] != '\0' && s[j] == t[k]; j++, k++);

                if(t[k] == '\0'){
                        return(TRUE);
                }
        }
        return(FALSE);
}

delay()
{
	sleep(2);
	return;
}

cu_lock(char *device)
{
char *node; 
int n;
	(void)strncpy(d_Lfile,"/var/tmp/LCK..",22);
	node=device;
	for(n=0;n<strlen(device);n++){
		node++;
		if( *node == '\057'){
			node++;
			break;
		}
	}
	(void)strcat(d_Lfile,node);
	return(d_gfp3=creat(d_Lfile,O_RDONLY));
}

cu_ulock()
{
	close(d_gfp3);	
	return(unlink(d_Lfile));
}

getdevice(char *device)
{
    char lastchar[2], tmp_dev[30];
    char lastnum[5], tmp_modem[30];
    int  i;
   
    lastchar[0] = 'H';
    lastchar[1] = '\0';
    strcpy(tmp_modem, "/tmp/modem");
    strcpy(tmp_dev, "/dev/acu");

    for (speed=EXTB; speed>B600; speed--)  {
        if (d_debug)  {
            fprintf(d_file, "\nTrying speed %d ... ", speed);
            fflush(d_file);
        }


        for (i=0; i<num_modem; i++)  {
            itoa(i, lastnum); 
            strcpy(d_modem, tmp_modem);
            strcat(d_modem, lastnum);
            strcpy(device, tmp_dev);
            strcat(device, lastchar);
            strcat(device, lastnum);

            if (d_debug)  {
                fprintf(d_file, "Trying %s %s\n", device, d_modem);
                fflush(d_file);
            }


            if ((link(device, d_modem)) == 0)   {
                return(0);     /*  Successful return  */
            }
            if (errno == ENOENT)  {
                if (d_debug)  {
                    fprintf(d_file, "\nENOENT for %s\n", device);
                    fflush(d_file);
                }
            }
        }
        lastchar[0]--;
    }
    if (d_debug)
        fprintf(d_file, "\nNo lines available\n");
    return (1);
}
 
itoa(inum, chnum)
int inum;
char *chnum;
{
    int i, j=0;
    chnum[0] = '\0';

    if (inum == 0)  {
        chnum[0] = '0';
        chnum[1] = '\0';
    }

    while (inum)  {
        for (j=strlen(chnum); j>=0; j--)
            chnum[j+1] = chnum[j];
        i = inum % 10;
        i += '0';
        chnum[0] = i;
        inum /= 10;
    }
}


