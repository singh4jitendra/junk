#if !defined(_NET_STUFF_H)
#define _NET_STUFF_H

#define INIT_COMM  0x01
#define TX_START   0x02
#define TX_END     0x03
#define RX_START   0x04
#define RX_END     0x05
#define FILE_START 0x06
#define FILE_DATA  0x07
#define FILE_END   0x08
#define CMD_ACK    0x09
#define END_COMM   0x0a
#define FREE_SPACE 0x0b

#define CMD_FAILED     0x00
#define CMD_SUCCESSFUL 0x01

#define FILE_TRANSFER_DONE 0
#define FILE_TRANSFER_RX   1
#define FILE_TRANSFER_TX   2

#define FSP_OK                     0
#define FSP_NO_FREE_SPACE          1
#define FSP_NO_INODES              2

#define FREE_SPACE_SPACE_AVAIL     0
#define FREE_SPACE_NO_SPACE_AVAIL  1
#define FREE_SPACE_NO_INODES_AVAIL 2

/* Redefine PIPE_BUF to match net_dial */
#ifdef PIPE_BUF
#undef PIPE_BUF
#endif

#define PIPE_BUF 5120

typedef struct {
   short op_code;
   char version[VERSIONLENGTH];
} INIT_COMM_PACKET;

typedef struct {
   short op_code;
} TX_START_PACKET;

typedef struct {
   short op_code;
} TX_END_PACKET;

typedef struct {
   short op_code;
} RX_START_PACKET;

typedef struct {
   short op_code;
} RX_END_PACKET;

typedef struct {
   short op_code;
   char filename[_POSIX_PATH_MAX];
   int32_t size;
} FILE_START_PACKET;

typedef struct {
   short op_code;
   int32_t bytes;
   char data[1024];
} FILE_DATA_PACKET_OLD;

typedef struct {
	short op_code;
	int32_t bytes;
	char data[PIPE_BUF - sizeof(short) - sizeof(int32_t)];
} FILE_DATA_PACKET;

typedef struct {
   short op_code;
} FILE_END_PACKET;

typedef struct {
   short op_code;
   short status;
   char message[255];
} CMD_ACK_PACKET;

typedef struct {
   short op_code;
} END_COMM_PACKET;

typedef struct {
	short op_code;
	int32_t space_amount;
	char space_measure;
	int32_t inode_amount;
	char inode_measure;
	u_int32_t total_size;
} FREE_SPACE_REQUEST_PACKET;

typedef struct {
   int total_rx_bytes;
   int total_tx_bytes;
   int rx_bytes;
   int tx_bytes;
   int state;
   time_t timer;
} THROUGHPUT_INFO;

#endif
