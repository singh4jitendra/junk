/*
 ******************************************************************************
 *
 * servicecheck.c - This program connects to a service to see if the service
 *                  is accepting connections.  By default, it sends a version
 *                  number to the server.  This version number is used by
 *                  Shared Inventory servers.  There is an option (-d) to
 *                  disconnect immediately after returning from the connect.
 *                  This prevents the version number from being sent.
 *
 * Command-line options:
 *                  d  - Disconnect immediately after connect; don't send
 *                       version number.
 *                  t  - Specify timeout value in seconds.
 *                  v  - Use specified version number.
 *
 * Return codes:
 *                  0  - Successful
 *                  1  - Syntax error on command line
 *                  2  - Invalid host name
 *                  3  - Invalid service name
 *                  4  - Error creating socket
 *                  5  - Error connecting to host
 *
 *
 * Changed: 12/22/04 mjb - Project 04041.  Changed a "send" call to a "write"
 *                         call.
 *
 ******************************************************************************
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>

#define FALSE              0
#define TRUE               1

#define DEFAULT_TIMEOUT    30
#define DEFAULT_VERSION    "00.00.00"

void sigalrm_handler(int);

/*
 ******************************************************************************
 *
 * Function:    main
 *
 * Description: This function processes the command line arguments and
 *              connects to the remote machine.  It is called automatically
 *              at startup.
 *
 * Parameters:  argc - int; number of command line arguments
 *              argv - char *[]; array containing the arguments
 *
 * Returns:     Returns an integer to the operating system.
 *                  0  - Successful
 *                  1  - Syntax error on command line
 *                  2  - Invalid host name
 *                  3  - Invalid service name
 *                  4  - Error creating socket
 *                  5  - Error connecting to host
 *
 ******************************************************************************
*/
main(int argc, char *argv[])
{
   int s;                                  /* socket descriptor              */
   int option;                             /* current command line option    */
   int index;                              /* index into argv                */
   int disconnect_immediately = FALSE;     /* exit after shutdown            */
   int timeout_value = DEFAULT_TIMEOUT;    /* timeout in seconds             */
   char *version_number = DEFAULT_VERSION; /* version number to transmit     */
   struct sigaction alarm_action;          /* signal action for SIGALRM      */
   struct hostent *hp;                     /* pointer to /etc/host entry     */
   struct servent *sp;                     /* pointer to /etc/services entry */
   struct sockaddr_in peeraddr;            /* address of remote machine      */
   extern char *optarg;                    /* pointer to option argument     */
   extern int optind;                      /* index of next option           */
   char buffer[9];                         /* transmit/receive buffer        */

   /* check to see if there are at least three parameters */
   if (argc < 3) {
      fprintf(stderr,
         "Usage: servicecheck [ -d -t <timeout> -v <version> ] host service\n");
      exit(1);    /* no parameters specified */
   }

   /* process the command line options */
   while ((option = getopt(argc, argv, "dt:v:")) != EOF) {
      switch (option) {
         case 'd':    /* disconnect after conversation is accepted */
            disconnect_immediately = TRUE;
            break;
         case 't':    /* specify a new timeout value */
            timeout_value = atol(optarg);
            if (timeout_value < 0) {
               fprintf(stderr, "timeout value must be 0 or greater\n");
               exit(1);
            }

            break;
         case 'v':    /* specify a new version number */
            version_number = optarg;
            break;
         default:
            exit(1);    /* bad option */
      }
   }

   /* set up the alarm handler to act as the timeout */
   alarm_action.sa_handler = sigalrm_handler;
   sigemptyset(&alarm_action.sa_mask);
   alarm_action.sa_flags = 0;
   sigaction(SIGALRM, &alarm_action, NULL);

   /* ensure that a service name was provided */
   index = optind;
   if (!strcmp(argv[index], "") && !strcmp(argv[index+1], "")) {
      fprintf(stderr,
         "Usage: servicecheck [ -d -t <timeout> -v <version> ] services\n");
      exit(1);    /* no service specified */
   }

   /* look up host name in /etc/hosts */
   if ((hp = gethostbyname(argv[index])) == NULL) {
      fprintf(stderr, "servicecheck: %s not in /etc/hosts\n", argv[index]);
      exit(2);
   }

   /* look up service in /etc/services */
   if ((sp = getservbyname(argv[index+1], "tcp")) == NULL) {
      fprintf(stderr, "servicecheck: %s not in /etc/services\n",
         argv[index+1]);
      exit(3);
   }

   /* construct the address of the remote machine */
   peeraddr.sin_family = AF_INET;
   peeraddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
   peeraddr.sin_port = sp->s_port;

   /* create the socket */
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("checkservice");
      exit(4);
   }

   /* connect to the remote machine */
   alarm(timeout_value);   /* set alarm for timeout */
   if (connect(s, &peeraddr, sizeof(peeraddr)) < 0) {
      perror("checkservice");
      exit(5);
   }
   alarm(0);               /* reset the alarm so it doesn't go off */

   if (!disconnect_immediately) {
      /* currently this will not do anything */
      write(s, version_number, 9); /* change to write - 04041 */
   }

   /* end the conversation and close the socket */
   shutdown(s, 2);
   close(s);

   return 0;   /* all is well */
}

/*
 ******************************************************************************
 *
 * Function:    sigalrm_handler
 *
 * Description: This function is called by the kernal in response to receiving
 *              the SIGALRM signal.
 *
 * Parameters:  information - int; additional information about the signal
 *
 * Returns:     Nothing
 *
 ******************************************************************************]
*/
void sigalrm_handler(int information)
{
   information ^= 1;   /* this line prevents lint from complaining    */
   return;             /* this handler is not required to do anything */
}
