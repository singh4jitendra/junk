#if defined(__hpux)
#   define _HPUX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <signal.h>
#include <string.h>

#ifdef _GNU_SOURCE
#include <signal.h>
#else
#include <siginfo.h>
#endif

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "net_client.h"
#include "net_stuff.h"
#include "net_lib.h"
#include "pidfile.h"
#include "setsignal.h"
#include "fs_rules.h"

int ls;   /* global for signal handler; treated as local otherwise */
char *config_file = "/usr/hlcp/netpoll.ini";
int program_return_code = 0;
int print_date = 1;

main(int argc, char *argv[])
{
   int option;
   char service[12];
   char protocol[12];
   char prepath[_POSIX_PATH_MAX];
   struct sockaddr_in local_address;
   extern char *optarg;
   int bindretry;

   while ((option = getopt(argc, argv, "bdi:ks")) != EOF) {
      switch (option) {
         case 'b':   /* run in background */
            run_in_bkgrnd = 1;
            break;
         case 'd':   /* write to debug log */
            debug_log = 1;
            print_msg("debug logging is set\n");
            break;
         case 'i':   /* use configuration file */
            config_file = optarg;
            break;
         case 'k':   /* kill dialup client */
            atexit(kill_dialup_client);
            break;
         case 's':   /* socket-level debugging */
            debug_socket = 1;
            break;
         default:
            print_msg("invalid option %c\n", option);
            fflush(stdout);
            program_return_code = 1;
            exit(program_return_code);
      }
   }

   if (debug_log && run_in_bkgrnd)
      printf("run in background option is set\n");

   if (!debug_socket)
      debug_socket = GetPrivateProfileInt("net_client", "debugsocket", 0,
         config_file);

   if (debug_log && debug_socket)
      printf("socket-level debugging is set\n");

   if (debug_log)
      printf("using configuration file %s\n", config_file);

   if (run_in_bkgrnd) {
      daemoninit();
      if (debug_log)
         printf("process running in background\n");
   }

   umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

   GetPrivateProfileString("netpoll", "service", service, sizeof(service),
      "NETPOLL", config_file);
   GetPrivateProfileString("netpoll", "protocol", protocol,
      sizeof(protocol), "tcp", config_file);
   GetPrivateProfileString("net_client", "path", prepath, sizeof(prepath),
      "/usr/hlcp", config_file);
   bindretry = GetPrivateProfileInt("net_client", "bindretry", 30,
      config_file);

   if (debug_log)
      printf("using service %s, protocol %s\n", service, protocol);

   print_msg("net_client Data XFER version 1.0 running\n");

#ifdef SA_SIGINFO
   setsignal(SIGUSR1, (void(*)(int))sigusr1, NULL, SA_SIGINFO, 0);
   setsignal(SIGUSR2, sigusr2, NULL, 0, 0);
   setsignal(SIGWINCH, (void(*)(int))sigwinch, NULL, SA_SIGINFO, 0);
#else
   setsignal(SIGUSR1, sigusr1, NULL, 0, 0);
   setsignal(SIGWINCH, sigwinch, NULL, 0, 0);
#endif

   setsignal(SIGTERM, sigterm, NULL, 0, 0);
   setsignal(SIGALRM, sigalrm, NULL, 0, 0);

   if (initaddress(&local_address, service, protocol) < 0) {
      print_msg("%s\\%s not in /etc/services\n", service, protocol);
      fflush(stdout);
      program_return_code = 11;
      exit(program_return_code);
   }

   if (debug_log)
      printf("server address initialized\n");

   while ((ls = getlistensocket(&local_address, 1)) < 0) {
      print_msg("error creating listen socket: %s\n", strerror(errno));
      sleep(bindretry);
   }

   if (debug_log)
      printf("listen socket was created successfully\n");

   setsocketdebug(ls, debug_socket);

   if (CreatePIDFile(prepath, "net_client", NULL) < 0) {
      print_msg("net_client: error creating pid file\n");
      program_return_code = 12;
      return program_return_code;
   }

   return wait_for_connection(ls, config_file);
}

int wait_for_connection(int ls, char *config_file)
{
   int s;
   struct sockaddr_in remote_address;
   int len = sizeof(remote_address);
   int timeout;
   struct timeval tv = { 5, 0 };

   timeout = GetPrivateProfileInt("netpoll", "timeout", 30, config_file);

   if ((s = accept(ls, (struct sockaddr *)&remote_address, &len)) < 0) {
      print_msg("net_client: %s\n", strerror(errno));
      close(ls);   /* close the listen socket */
      program_return_code = 13;
      return program_return_code;
   }
   else {
      set_timeout(timeout);
      setsocketdebug(s, debug_socket);
      program_return_code = process_communications(s, config_file);
      print_msg("INFO: Closing listen socket.\n");
      close(ls);   /* close the listen socket */
   }

   print_msg("INFO: Flushing standard out.\n");
   fflush(stdout);

   print_msg("INFO: Exiting with code %d.\n", program_return_code);

   setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
   setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));

   return program_return_code;
}

int process_communications(int s, char *config_file)
{
   char buffer[2048];
   char path[_POSIX_PATH_MAX];
   char prepath[_POSIX_PATH_MAX];
   short *p_opcode;
   short op_code;
   int index = 0;
   int count = 0;
   INIT_COMM_PACKET *icp;
   THROUGHPUT_INFO info;
   FNAMES *head = NULL;
	struct fsrules  fsr;
	struct statvfs vfs;
	FREE_SPACE_REQUEST_PACKET *fsrp;
	int response;
	extern int data_packet_size;

   memset(&info, 0, sizeof(info));

   if (debug_log)
      printf("Entering process_communications()\n");

   GetPrivateProfileString("net_client", "path", prepath, sizeof(prepath),
      "/usr/hlcp", config_file);

   while (recv_packet(s, buffer, sizeof(buffer)) > 0) {
      p_opcode = (short *)buffer;
      op_code = ntohs(*p_opcode);

      switch(op_code) {
         case INIT_COMM:
            if (debug_log)
               printf("rx'd INIT_COMM message\n");

            count++;

            icp = (INIT_COMM_PACKET *)buffer;
            for (index = 0; index < NUM_VERSIONS_SUPPORTED; index++) {
               if (!strcmp(supported_versions[index], icp->version)) {
                  if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
                     return -1;
                  break;
               }
               else if ((index + 1) == NUM_VERSIONS_SUPPORTED) {
                  if (send_cmd_ack(s, CMD_ACK, CMD_FAILED,
                      "invalid version %s\n", icp->version) < 0)
                     return -1;
               }
            }

				if (strcmp(supported_versions[index], "01.00.00") == 0) {
					data_packet_size = 1024;
				}
				else {
					data_packet_size = PIPE_MAX - sizeof(short) - sizeof(int32_t);
				}

            break;
         case TX_START:
            if (debug_log)
               printf("rx'd TX_START message\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
               return -1;

            sprintf(path, "%s/in", prepath);
            if (receive_all_files(s, path, &info) < 0) {
               print_msg("file transfer failed: %s\n", strerror(errno));
               return -1;
            }

            break;
         case RX_START:
            if (debug_log)
               printf("rx'd RX_START message\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
               return -1;

            sprintf(path, "%s/out", prepath);
            if ((head = build_file_list(path)) != NULL) {
               if (transmit_all_files(s, path, head, &info) < 0) {
                  send_end_comm(s);
                  return -1;
               }

               free_list(head);
            }

            if (send_rx_end(s) < 0)
               return -1;

            break;
         case RX_END:
            if (debug_log)
               printf("rx'd RX_END message\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
               return -1;

            break;
         case END_COMM:
            if (debug_log)
               printf("rx'd END_COMM message\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
               return -1;

				close(s);

            return 0;
			case FREE_SPACE:
				if (debug_log)
					printf("DEBUG: process_communications: FREE_SPACE message\n");

				fsrp = (FREE_SPACE_REQUEST_PACKET *)buffer;
				fsr.space_amount = (int)ntohl(fsrp->space_amount);
				fsr.space_measure = fsrp->space_measure;
				fsr.inode_amount = (int)ntohl(fsrp->inode_amount);
				fsr.inode_measure = fsrp->inode_measure;

				if (debug_log) {
					printf("DEBUG: process_communications: fsr.id \"%s\"\n",
						fsr.id);
					printf("DEBUG: process_communications: fsr.space_amount: %d\n",
						fsr.space_amount);
					printf("DEBUG: process_communications: fsr.space_measure: %c\n",
						fsr.space_measure);
					printf("DEBUG: process_communications: fsr.inode_amount: %d\n",
						fsr.inode_amount);
					printf("DEBUG: process_communications: fsr.inode_measure: %c\n",
						fsr.inode_measure);
				}

				if (statvfs("/usr/hlcp", &vfs) < 0) {
					if (send_cmd_ack(s, CMD_ACK, CMD_FAILED, "error statusing /usr/hlcp") < 0)
						return -1;
				}

				if (!fs_validate_free_space(&fsr, &vfs, ntohl(fsrp->total_size))) {
					if (debug_log)
						printf("DEBUG: process_communications: FSP_NO_FREE_SPACE\n");

					response = FSP_NO_FREE_SPACE;
				}
				else if (!fs_validate_free_inodes(&fsr, &vfs)) {
					if (debug_log)
						printf("DEBUG: process_communications: FSP_NO_INODES\n");

					response = FSP_NO_INODES;
				}
				else {
					if (debug_log)
						printf("DEBUG: process_communications: FSP_OK\n");

					response = FSP_OK;
				}

            if (send_cmd_ack(s, CMD_ACK, response, NULL) < 0)
               return -1;

				break;
         default:
            if (debug_log)
               printf("rx'd message %d\n", op_code);

            (void)send_cmd_ack(s, CMD_ACK, CMD_FAILED,
               "unexpected command - %d\n", op_code);
            return -1;
      }
   }

   print_msg("Error: %s\n", strerror(errno));

   if (debug_log)
      printf("Exiting process_communications()\n");

	return -1;
}

#ifdef SA_SIGINFO
void sigusr1(int signo, struct siginfo *p_siginfo)
{
   close(ls);
   if (p_siginfo != NULL)
      kill( p_siginfo->si_pid, SIGUSR2);

   _exit(14);
}

void sigwinch(int signo, struct siginfo *p_siginfo)
{
   close(ls);
   if (p_siginfo != NULL)
      kill( p_siginfo->si_pid, SIGUSR2);

   _exit(0);
}
#else
void sigusr1(int signo)
{
   close(ls);
   _exit(14);
}

void sigwinch(int signo)
{
   close(ls);
   _exit(0);
}
#endif

volatile sig_atomic_t signal_acked = 0;

void kill_dialup_client(void)
{
   char dialup_client[_POSIX_PATH_MAX];
   char prepath[_POSIX_PATH_MAX];
   char dc_pidfile[_POSIX_PATH_MAX];
   pid_t dialup_pid;
   int attempt = 0;
   int signo;

   GetPrivateProfileString("net_client", "path", prepath, sizeof(prepath),
      "/usr/hlcp", config_file);
   GetPrivateProfileString("net_client", "dialup_client", dialup_client,
      sizeof(dialup_client), "client", config_file);

   sprintf(dc_pidfile, "%s/%s.pid", prepath, dialup_client);
   if ((dialup_pid = ReadPIDFile(dc_pidfile)) < 2)
      return;

   signo = (program_return_code != 0) ? SIGUSR1 : SIGWINCH;

#ifdef SA_SIGINFO
   while (attempt < 3 && !signal_acked) {
      if (kill( dialup_pid, signo) < 0)
         return;

      if (!signal_acked)
         sleep(5);

      attempt++;
   }

   if (!signal_acked)
      kill( dialup_pid, SIGKILL);
#else
   kill( dialup_pid, signo);
#endif
}

#ifdef SA_SIGINFO
void sigusr2(int signo)
{
   signal_acked = 1;
}
#endif

void sigterm(int signo)
{
   close(ls);
   exit(14);
}

void sigalrm(int signo)
{
   print_msg("INFO: Communications Timeout\n");
}
