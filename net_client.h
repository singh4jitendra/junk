#if !defined(_NET_CLIENT_H)
#define _NET_CLIENT_H

int wait_for_connection(int, char *);
int process_communications(int, char *);
void kill_dialup_client(void);
void sigterm(int);
void sigalrm(int);

#if defined(SA_SIGINFO)
void sigusr1(int, struct siginfo *);
void sigusr2(int);
void sigwinch(int, struct siginfo *);
#else
void sigusr1(int);
void sigwinch(int);
#endif

int debug_log = 0;
int debug_socket = 0;
int run_in_bkgrnd = 0;
char *host = "";

#define NUM_VERSIONS_SUPPORTED 2
const char *supported_versions[NUM_VERSIONS_SUPPORTED] = {
	"02.00.00",
   "01.00.00"
};

#endif
