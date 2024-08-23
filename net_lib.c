#if defined(__hpux)
#   define _HPUX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _XOPEN_SOURCE_EXTENDED 1
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>

#include "common.h"
#include "net_stuff.h"
#include "net_lib.h"
#include "fs_rules.h"

#if defined(_DEBUG_THIS_)
#   define send_packet write
#   define recv_packet read
#endif

#define NUM_VERSIONS_SUPPORTED 2

extern int debug_log;
extern char *supported_versions[];
extern char *host;
int timeout = 30;   /* timeout in seconds, default 30 */
u_int32_t total_size;
int data_packet_size = 0;

int transmit_all_files(int s, char *tx_path, FNAMES *head,
   THROUGHPUT_INFO *p_info)
{
   int32_t filesize;
   FNAMES *current = NULL;
   char sent_name[_POSIX_PATH_MAX];
   char real_name[_POSIX_PATH_MAX];

   p_info->state = FILE_TRANSFER_TX;
   p_info->tx_bytes = 0;
   p_info->timer = 0;

   if (debug_log)
      printf("Entering transmit_all_files()\n");

   if (debug_log)
      printf("transmit path is %s\n", tx_path);

   current = head;
   while (current != NULL) {
      if (send_file_start(s, tx_path, current->name, &filesize) < 0)
         return -1;

      print_msg("INFO: START_SENDING %s/%s\n", tx_path, current->name);
      fflush(stdout);

      if (transmit_file(s, tx_path, current->name, p_info) < 0)
         return -1;

      if (send_file_end(s) < 0)
         return -1;

      sprintf(real_name, "%s/%s", tx_path, current->name);
      sprintf(sent_name, "%s/%s.sent", tx_path, current->name);
      if (rename(real_name, sent_name) < 0) {
         print_msg("ERROR: error renaming %s: \n", sent_name);
         return -1;
      }

      print_msg("INFO: SEND_COMPLETE %s %d\n", sent_name, filesize);
      fflush(stdout);

      current = current->next;
   }

   if (debug_log)
      printf("Exiting transmit_all_files()\n");

   return 0;
}

int receive_all_files(int s, char *path, THROUGHPUT_INFO *p_info)
{
   int fd;
   int bytes;
   int32_t filesize;
   short *p_opcode;
   short op_code;
   char buffer[1026];
   char progname[_POSIX_PATH_MAX];
   char inname[_POSIX_PATH_MAX];
   char noinname[_POSIX_PATH_MAX];
   FILE_START_PACKET *fsp;
   RX_END_PACKET *rep;

   p_info->state = FILE_TRANSFER_RX;

   if (debug_log)
      printf("Entering receive_all_files()\n");

   while ((bytes = recv_packet(s, buffer, sizeof(buffer))) > 0) {
      p_opcode = (short *)buffer;
      op_code = ntohs(*p_opcode);

      switch (op_code) {
         case FILE_START:
            print_msg("INFO: rx'd FILE_START packet\n");
            fsp = (FILE_START_PACKET *)buffer;
            filesize = ntohl(fsp->size);
            sprintf(progname, "%s/%s.prog", path, fsp->filename);
            sprintf(inname, "%s/%s.in", path, fsp->filename);
            sprintf(noinname, "%s/%s", path, fsp->filename);

            if ((fd = creat(progname, READWRITE)) < 0) {
               send_cmd_ack(s, CMD_ACK, CMD_FAILED, "%s\n", strerror(errno));
               return -1;
            }

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0) {
               close(fd);
               return -1;
            }

           print_msg("INFO: START_RECEIVE: %s\n", noinname);
            fflush(stdout);

            if (receive_file(s, fd, progname, inname, &filesize, p_info) < 0) {
               print_msg("ERROR: error receiving file %s\n", progname);
               return -1;
            }

            break;
         case TX_END:
            print_msg("INFO: rx'd TX_END packet\n");
            if (debug_log)
               printf("Exiting receive_all_files()\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0) {
               print_msg("ERROR: error sending ack: %s\n", strerror(errno));
               return -1;
            }

            return 0;
         case RX_END:
            print_msg("INFO: rx'd RX_END packet\n");
            if (debug_log)
               printf("Exiting receive_all_files()\n");

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0) {
               print_msg("ERROR: error sending ack: %s\n", strerror(errno));
               return -1;
            }

            return 0;
         default:
            print_msg("ERROR: Unexpected command %d\n", op_code);
            send_end_comm(s);
            return -1;
      }
   }

   if (debug_log)
      printf("Exiting receive_all_files()\n");

   return bytes;
}

int send_init_comm(int s, int *index)
{
   INIT_COMM_PACKET icp;
   CMD_ACK_PACKET cap;
   int save_errno;

   if (debug_log)
      printf("Enter send_init_comm()\n");

   icp.op_code = htons(INIT_COMM);

   while (1) {
      strcpy(icp.version, supported_versions[*index]);

      if (debug_log)
         printf("sending version %s\n", supported_versions[*index]);

      /* send the software version */
      if (send_packet(s, &icp, sizeof(icp)) < 0) {
         print_msg("ERROR: error sending packet: \n");
         return -1;
      }

      /* receive response from remote */
      if (recv_packet(s, &cap, sizeof(cap)) < 0) {
         print_msg("ERROR: error receiving packet: %s\n", strerror(errno));
         return -1;
      }

#if !defined(_DEBUG_THIS_)
      if (ntohs(cap.status) == CMD_SUCCESSFUL) {
         if (debug_log)
            printf("using version %s\n", icp.version);

         break;
      }
      else {
         if (debug_log)
            printf(cap.message);

         (*index)++;
         if (*index >= NUM_VERSIONS_SUPPORTED) {
            if (debug_log)
               printf(cap.message);

            print_msg(cap.message);
            return -1;
         }
      }
#else
      if (debug_log)
         printf("using version %s\n", icp.version);

      break;
#endif
   }

	if (strcmp(supported_versions[*index], "01.00.00") == 0) {
		data_packet_size = 1024;
	}
	else {
		data_packet_size = PIPE_MAX - sizeof(short) - sizeof(int32_t);
	}

   if (debug_log)
      printf("Exit send_init_comm()\n");

   return 0;
}

int send_tx_start(int s)
{
   TX_START_PACKET tsp;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_tx_start()\n");

   tsp.op_code = htons(TX_START);

   /* tell remote to enter receive mode */
   if (send_packet(s, &tsp, sizeof(tsp)) < 0) {
      print_msg("ERROR: error sending packet: %s\n", strerror(errno));
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: %s\n", strerror(errno));
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      print_msg("ERROR: %s\n", cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_tx_start()\n");

   return 0;
}

int send_tx_end(int s)
{
   TX_END_PACKET tep;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_tx_end()\n");

   tep.op_code = htons(TX_END);

   /* tell remote that all files were transmitted */
   if (send_packet(s, &tep, sizeof(tep)) < 0) {
      print_msg("ERROR: error sending packet: %s\n", strerror(errno));
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: %s\n", strerror(errno));
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      print_msg("ERROR: %s\n", cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_tx_end()\n");

   return 0;
}

int send_rx_start(int s)
{
   RX_START_PACKET rsp;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_rx_start()\n");

   rsp.op_code = htons(RX_START);

   /* tell remote to enter transmit mode */
   if (send_packet(s, &rsp, sizeof(rsp)) < 0) {
      print_msg("ERROR: error sending packet: %s\n", strerror(errno));
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: %s\n", strerror(errno));
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      print_msg("ERROR: %s\n", cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_rx_start()\n");

   return 0;
}

int send_rx_end(int s)
{
   RX_END_PACKET rep;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_rx_end()\n");

   rep.op_code = htons(RX_END);
   print_msg("INFO: sent RX_END command\n");

   /* tell remote that all files were received */
   if (send_packet(s, &rep, sizeof(rep)) < 0) {
      print_msg("ERROR: error sending packet: %s\n", strerror(errno));
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: %s\n", strerror(errno));
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      print_msg("ERROR: %s\n", cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_rx_end()\n");

   return 0;
}

int send_file_start(int s, char *path, char *filename, int32_t *filesize)
{
   FILE_START_PACKET sfp;
   CMD_ACK_PACKET cap;
   char fullname[_POSIX_PATH_MAX];
   struct stat fs;

   if (debug_log)
      printf("Enter send_file_start()\n");

   sprintf(fullname, "%s/%s", path, filename);

   if (stat(fullname, &fs) < 0)
      return -1;

   sfp.op_code = htons(FILE_START);
   strcpy(sfp.filename, filename);
   sfp.size = htonl((int32_t)fs.st_size);
   *filesize = fs.st_size;

   /* send filename to server */
   if (send_packet(s, &sfp, sizeof(sfp)) < 0) {
      print_msg("ERROR: error sending packet: \n");
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: \n");
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_file_start()\n");

   return 0;
}

int send_file_data(int s, char *buffer, int32_t bytes)
{
	size_t ts;
   FILE_DATA_PACKET fdp;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_file_data()\n");

   fdp.op_code = htons(FILE_DATA);
   fdp.bytes = htonl(bytes);
   memcpy(fdp.data, buffer, bytes);
   if (debug_log)
      printf("tx'd %d bytes\n", bytes);

	if (data_packet_size == 1024)
		ts = sizeof(FILE_DATA_PACKET_OLD);
	else
		ts = sizeof(FILE_DATA_PACKET);

   /* send filename to server */
   if (send_packet(s, &fdp, ts) < 0) {
      print_msg("ERROR: error sending packet: \n");
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: \n");
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      printf(cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_file_data()\n");

   return 0;
}

int send_file_end(int s)
{
   FILE_END_PACKET fep;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_file_end()\n");

   fep.op_code = htons(FILE_END);

   /* send EOF to server */
   if (send_packet(s, &fep, sizeof(fep)) < 0) {
      print_msg("ERROR: error sending packet: \n");
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   /* receive response from remote */
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      print_msg("ERROR: error receiving packet: \n");
      return -1;
   }

   if (ntohs(cap.status) != CMD_SUCCESSFUL) {
      printf(cap.message);
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_file_end()\n");

   return 0;
}

int send_cmd_ack(int s, int op_code, int status, char *message, ...)
{
   va_list ap;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Entering send_cmd_ack()\n");

   memset(&cap, 0, sizeof(cap));

   cap.op_code = htons(op_code);
   cap.status = htons(status);

   if (message != NULL) {
      va_start(ap, message);
      vsprintf(cap.message, message, ap);
      va_end(ap);
   }

   if (debug_log)
      printf("Exiting send_cmd_ack()\n");

   return send_packet(s, &cap, sizeof(cap));
}

int send_end_comm(int s)
{
   END_COMM_PACKET ecp;
   CMD_ACK_PACKET cap;

   if (debug_log)
      printf("Enter send_end_comm()\n");

   ecp.op_code = htons(END_COMM);

   if (send_packet(s, &ecp, sizeof(ecp)) < 0) {
      return -1;
   }

#if !defined(_DEBUG_THIS_)
   if (recv_packet(s, &cap, sizeof(cap)) < 0) {
      return -1;
   }
#endif

   if (debug_log)
      printf("Exit send_end_comm()\n");

   return 0;
}

int transmit_file(int s, char *path, char *filename, THROUGHPUT_INFO *p_info)
{
   int fd;
   int32_t bytes;
   char fullname[_POSIX_PATH_MAX];
   char buffer[PIPE_MAX+4];

   sprintf(fullname, "%s/%s", path, filename);

   if ((fd = open(fullname, O_RDONLY)) < 0) {
      print_msg("ERROR: error opening file %s: %s\n", fullname,
         strerror(errno));
      return -1;
   }

   while ((bytes = read(fd, buffer, data_packet_size)) > 0) {
      if (send_file_data(s, buffer, bytes) < 0) {
         close(fd);
         return -1;
      }

#ifdef _GENERATE_THROUGHPUT_DATA
      p_info->tx_bytes += bytes;
      if (time(NULL) + 10 > p_info->timer)
         net_throughput(p_info);
#endif
   }

   close(fd);

   if (bytes < 0) {
      print_msg("ERROR: error reading %s: \n", fullname);
      return -1;
   }

   return 0;
}

int receive_file(int s, int fd, char *oldname, char *newname, int32_t *filesize,
    THROUGHPUT_INFO *p_info)
{
   int bytes;
   int32_t file_bytes;
   short op_code;
   short *p_code;
   char buffer[PIPE_BUF+4];
   FILE_DATA_PACKET *fdp;
   FILE_END_PACKET *fep;

   while ((bytes = recv_packet(s, buffer, sizeof(buffer))) > 0) {
      p_code = (short *)buffer;
      op_code = ntohs(*p_code);

      switch (op_code) {
         case FILE_DATA:   /* file data transmitted */
            fdp = (FILE_DATA_PACKET *)buffer;
            file_bytes = ntohl(fdp->bytes);
            if (debug_log)
               printf("rx'd %d bytes\n", file_bytes);

            p_info->rx_bytes += file_bytes;

            if (write(fd, fdp->data, file_bytes) < 0) {
               send_cmd_ack(s, CMD_ACK, CMD_FAILED, "%s\n", strerror(errno));
               close(fd);
               return -1;
            }

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0) {
               close(fd);
               return -1;
            }

            break;
         case FILE_END:    /* end of file transmitted */
            print_msg("INFO: rx'd FILE_END packet\n");
            fep = (FILE_END_PACKET *)buffer;
            close(fd);

           print_msg("INFO: RECEIVE_COMPLETE %s %d\n", newname,
               *filesize);
            fflush(stdout);

            if (send_cmd_ack(s, CMD_ACK, CMD_SUCCESSFUL, NULL) < 0)
               return -1;

            if (rename(oldname, newname) < 0) {
               send_cmd_ack(s, CMD_ACK, CMD_FAILED, "%s\n", strerror(errno));
               return -1;
            }

            return 0;
         case END_COMM:    /* branch ending communications */
            printf("rx'd END_COMM packet\n");
            close(fd);
            return -1;
         default:
            close(fd);
            return -1;
      }

#ifdef _GENERATE_THROUGHPUT_DATA
      if (time(NULL) + 10 > p_info->timer)
         net_throughput(p_info);
#endif
   }
}

void net_throughput(THROUGHPUT_INFO *p_info)
{
   time_t interval;

   interval = (time(NULL) - p_info->timer) + 10;
   if (interval <= 0)
      interval = 1;

   p_info->total_tx_bytes += p_info->tx_bytes;
   p_info->total_rx_bytes += p_info->rx_bytes;

   if (p_info->state == FILE_TRANSFER_DONE)
     print_msg("BRANCH: %s THROUGH: r:done t:done %uk/%uk\n", host, 
         p_info->total_rx_bytes, p_info->total_tx_bytes);
   else if (p_info->state == FILE_TRANSFER_RX)
     print_msg("BRANCH: %s THROUGH: r:%04d t:0000 %uk/%uk\n", host, 
         p_info->rx_bytes/interval, p_info->total_rx_bytes,
         p_info->total_tx_bytes);
   else if (p_info->state == FILE_TRANSFER_TX)
     print_msg("BRANCH: %s THROUGH: r:0000 t:%04d %uk/%uk\n", host, 
         p_info->tx_bytes/interval, p_info->total_rx_bytes,
         p_info->total_tx_bytes);

   fflush(stdout);

   p_info->tx_bytes = 0;
   p_info->rx_bytes = 0;
   p_info->timer = time(NULL) + 10;
}

void insert_file_alpha(FNAMES **head, FNAMES *fn)
{
   FNAMES *np;
   FNAMES *lp;

   if (debug_log)
     print_msg("Entering insert_file_alpha()\n");

   if (*head == NULL) {
      *head = fn;
      fn->next = NULL;
      return;
   }

   if (strcmp(fn->name, (*head)->name) < 1) {
      fn->next = *head;
      (*head) = fn;
      return;
   }

   lp = *head;
   np = (*head)->next;

   while (np != NULL) {
      if (strcmp(fn->name, np->name) < 1) {
         fn->next = np;
         lp->next = fn;

         if (debug_log)
           print_msg("Exiting insert_file_alpha()\n");

         return;
      }

      lp = np;
      np = np->next;
   }

   lp->next = fn;
   fn->next = NULL;

   if (debug_log)
     print_msg("Exiting insert_file_alpha()\n");
}

FNAMES *build_file_list(char *tx_path)
{
   DIR *tx_dir;
   struct dirent *entry;
   FNAMES *head = NULL;
   FNAMES *current = NULL;
	struct stat fs;
	char fn[PATH_MAX];

   if (debug_log)
     print_msg("Entering build_file_list()\n");

   if ((tx_dir = opendir(tx_path)) == NULL)
      return NULL;

	total_size = 0;
   while ((entry = readdir(tx_dir)) != NULL) {
      if (debug_log)
        print_msg("Checking %s\n", entry->d_name);

      if (file_pattern_match(entry->d_name, "*.sent") || !strcmp(entry->d_name, ".") ||
          !strcmp(entry->d_name, ".."))
         continue;

		memset(&fs, 0, sizeof(fs));
		memset(fn, 0, sizeof(fn));

		sprintf(fn, "%s/%s", tx_path, entry->d_name);
		if (stat(fn, &fs) != -1) {
			total_size += fs.st_size;
		}

      if (debug_log)
        print_msg("Allocating %d bytes for FNAME structure\n",
            sizeof(FNAMES));

      if ((current = malloc(sizeof(FNAMES))) == NULL) {
        print_msg("ERROR: no memory to build file list\n");
         exit(EXIT_NO_MEM);
      }

      if (debug_log)
        print_msg("Allocating %d bytes for filename\n",
            strlen(entry->d_name)+1);

      if ((current->name = malloc(strlen(entry->d_name)+1)) == NULL) {
        print_msg("ERROR: no memory to build file list\n");
         exit(EXIT_NO_MEM);
      }

      strcpy(current->name, entry->d_name);
      insert_file_alpha(&head, current);
   }

   closedir(tx_dir);

   if (debug_log)
     print_msg("Exiting build_file_list()\n");

   return head;
}

void free_list(FNAMES *head)
{
   FNAMES *current;

   current = head->next;
   while (head != NULL) {
      free(head->name);
      free(head);
      head = current;
      if (current == NULL)
         break;
      current = current->next;
   }
}

void print_msg(char *fmt, ...)
{
   va_list ap;
   time_t the_time;
   struct tm *tm;
   extern int print_date;

   if (print_date) {
      time(&the_time);
      tm = localtime(&the_time);
      fprintf(stdout, "%02d/%02d/%02d %02d:%02d:%02d ", tm->tm_year%100,
         tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
   }

   va_start(ap, fmt);
   vfprintf(stdout, fmt, ap);
   va_end(ap);
}

int recv_packet(int s, void *buffer, size_t buffsize)
{
    int total = 0;
    short tmpsize;  /* remote side sends a short, size_t is now a long */

	if (debug_log) {
		printf("DEBUG: recv_packet: **** ENTER ****\n");
		printf("DEBUG: recv_packet: s: %d\n", s);
		printf("DEBUG: recv_packet: buffer: 0x%08x\n", (unsigned long)buffer);
		printf("DEBUG: recv_packet: buffsize: %d\n", buffsize);
	}

    if (recv_data(s, (char *)&tmpsize, sizeof(tmpsize), 0) < 0) {
		if (debug_log) {
			printf("DEBUG: recv_packet: EOF while reading packet size!\n");
			printf("DEBUG: recv_packet: **** EXIT - FAIL ****\n");
		}

      return -1;
    }

	 if (debug_log)
		printf("DEBUG: recv_packet: tmpsize: %d\n", tmpsize);

	if ((size_t)tmpsize > buffsize) {
		printf("ERROR: recv_packet: buffer too small...aborting!\n");
		return -1;
	}
	
    tmpsize = endianswap(tmpsize);
    buffsize = (size_t)tmpsize;

	 if (debug_log)
		printf("DEBUG: recv_packet: buffsize: %d\n", buffsize);

    if ((total = recv_data(s, buffer, buffsize, 0 )) < 0) {
		if (debug_log) {
			printf("DEBUG: recv_packet: EOF while reading packet information!\n");
			printf("DEBUG: recv_packet: **** EXIT - FAIL ****\n");
		}

      return -1;
    }

	if (debug_log) {
		printf("DEBUG: recv_packet: **** EXIT - SUCCESS ****\n");
	}

    return total;
}

size_t send_packet(int s, void *buffer, size_t buffsize)
{
	short temp = endianswap((short)buffsize);

	if (debug_log) {
		printf("DEBUG: send_packet: **** ENTER ****\n");
		printf("DEBUG: send_packet: s: %d\n", s);
		printf("DEBUG: send_packet: buffer: 0x%08x\n", (unsigned long)buffer);
		printf("DEBUG: send_packet: buffsize: %d\n", buffsize);
	}

	if (send_data(s, (char *)&temp, sizeof(short), 0) < sizeof(short)) {
		if (debug_log) {
			printf("DEBUG: send_packet: EOF while reading packet size!\n");
			printf("DEBUG: send_packet: **** EXIT - FAIL ****\n");
		}

		return -1;
	}

	if (send_data(s, (char *)buffer, buffsize, 0) < buffsize) {
		if (debug_log) {
			printf("DEBUG: send_packet: EOF while reading packet information!\n");
			printf("DEBUG: send_packet: **** EXIT - FAIL ****\n");
		}

		return -1;
	}

	if (debug_log) {
		printf("DEBUG: send_packet: **** EXIT - SUCCESS ****\n");
	}

	return buffsize;
}

int recv_data(int s, void *buffer, size_t buffsize, int flags)
{
   int numbytes = 0;
   int total = 0;
   char *buf;

   buf = (char *)buffer;
   while (buffsize > 0) {
      alarm(timeout);
      if ((numbytes = recv(s, buf, buffsize, flags)) < 0) {
         alarm(0);
         return -1;
      }
      else if (numbytes == 0) {
         alarm(0);
         return total;
      }

      alarm(0);

      buffsize -= numbytes;
      buf += numbytes;
      total += numbytes;
   }

   return total;
}

int send_data(int s, void *buffer, size_t buffsize, int flags)
{
   int numbytes;
   int total = 0;
   char *buf;

   /* loop until all data is sent or an error has occurred */
   /* EWOULDBLOCK and EINTR errors are ignored */
   buf = (char *)buffer;
   while (buffsize > 0) {
      alarm(timeout);
      if ((numbytes = send(s, buf, buffsize, flags)) < 0) {
         alarm(0);
         return -1;
      }
      else if (numbytes == 0) {
         alarm(0);
         return total;
      }

      alarm(0);

      buffsize -= numbytes;
      buf += numbytes;
      total += numbytes;
   }

   total += numbytes;

   return total;
}

int send_free_space_check(int s, char * host)
{
	int rv;
	CMD_ACK_PACKET cap;
	struct fsrules * fsr;
	FREE_SPACE_REQUEST_PACKET fsp;

	if (debug_log) {
		printf("DEBUG: send_free_space_check: **** ENTER ****\n");
		printf("DEBUG: send_free_space_check: s: %d\n", s);
		printf("DEBUG: send_free_space_check: host: \"%s\"\n", host);
	}

	fsr = fs_find_rule_by_id(host);
	if (fsr == NULL) {
		fsr = fs_find_rule_by_id(FS_DEFAULT_IDENTIFIER);
		if (fsr == NULL) {
			if (debug_log)
				printf("DEBUG: send_free_space_check: **** EXIT - FAIL ****\n");

			return -1;
		}
	}

	if (debug_log) {
		printf("DEBUG: send_free_space_check: fsr->id \"%s\"\n", fsr->id);
		printf("DEBUG: send_free_space_check: fsr->space_amount: %d\n",
			fsr->space_amount);
		printf("DEBUG: send_free_space_check: fsr->space_measure: %c\n",
			fsr->space_measure);
		printf("DEBUG: send_free_space_check: fsr->inode_amount: %d\n",
			fsr->inode_amount);
		printf("DEBUG: send_free_space_check: fsr->inode_measure: %c\n",
			fsr->inode_measure);
		printf("DEBUG: send_free_space_check: total_size: %d\n", total_size);
	}

	fsp.op_code = htons(FREE_SPACE);
	fsp.space_amount = htonl((int32_t)(fsr->space_amount));
	fsp.space_measure = fsr->space_measure;
	fsp.inode_amount = htonl((int32_t)(fsr->inode_amount));
	fsp.inode_measure = fsr->inode_measure;
	fsp.total_size = htonl(total_size);

	if (debug_log)
		printf("DEBUG: send_free_space_check: sending FREE_SPACE packet\n");

	if (send_packet(s, &fsp, sizeof(fsp)) < 0) {
			if (debug_log)
				printf("DEBUG: send_free_space_check: **** EXIT - FAIL ****\n");

		return -1;
	}

	if (debug_log)
		printf("DEBUG: send_free_space_check: waiting for response\n");

#if !defined(_DEBUG_THIS)
	if (recv_packet(s, &cap, sizeof(cap)) < 0) {
			if (debug_log)
				printf("DEBUG: send_free_space_check: **** EXIT - FAIL ****\n");

		return -1;
	}
#endif

	switch (ntohs(cap.status)) {
		case FSP_OK:
			rv = 0;
			print_msg("INFO: %s free space check successful!\n", host);
			break;
		case FSP_NO_FREE_SPACE:
			rv = -1;
			print_msg("ERROR: disk space low\n", host);
			break;
		case FSP_NO_INODES:
			rv = -1;
			print_msg("ERROR: number of inodes low\n", host);
			break;
	}

	if (debug_log)
		printf("DEBUG: send_free_space_check: **** EXIT - SUCCESS ****\n");
		
	return rv;
}

void set_timeout(int new_timeout)
{
   timeout = new_timeout;
}
