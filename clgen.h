#ifndef _CLGEN_H_
#define _CLGEN_H_

#define GTA_TRANS_TYPE            0
#define GTA_HOST_NAME             1
#define GTA_CLIENT_NAME           2
#define GTA_OPERATION_MODE        3
#define GTA_USER_NAME             4
#define GTA_HOST_AREA             5
#define GTA_HOST_PROCESS          6
#define GTA_CLIENT_SND_FILE_NAME  7
#define GTA_HOST_RCV_FILE_NAME    8
#define GTA_HOST_RCV_FILE_PERM    9
#define GTA_HOST_RCV_FILE_OWNER   10
#define GTA_HOST_RCV_FILE_GROUP   11
#define GTA_HOST_SND_FILE_NAME    12
#define GTA_CLIENT_RCV_FILE_NAME  13
#define GTA_CLIENT_RCV_FILE_PERM  14
#define GTA_CLIENT_RCV_FILE_OWNER 15
#define GTA_CLIENT_RCV_FILE_GROUP 16
#define GTA_IO_FLAG               17
#define GTA_SPECIAL_SWITCHES      18
#define GTA_DETAIL                19

char *suboptions[] = {
   "GTA-TRANS-TYPE", "GTA-HOST-NAME", "GTA-CLIENT-NAME", "GTA-OPERATION-MODE",
   "GTA-USER-NAME", "GTA-HOST-AREA", "GTA-HOST-PROCESS",
   "GTA-CLIENT-SND-FILE-NAME", "GTA-HOST-RCV-FILE-NAME",
   "GTA-HOST-RCV-FILE-PERM", "GTA-HOST-RCV-FILE-OWNER",
   "GTA-HOST-RCV-FILE-GROUP", "GTA-HOST-SND-FILE-NAME",
   "GTA-CLIENT-RCV-FILE-NAME", "GTA-CLIENT-RCV-FILE-PERM",
   "GTA-CLIENT-RCV-FILE-OWNER", "GTA-CLIENT-RCV-FILE-GROUP",
   "GTA-IO-FLAG", "GTA-SPECIAL-SWITCHES", "GTA-DETAIL", NULL
};

char *hostname = NULL;
char *user = NULL;
char *script = NULL;

/** 50128 */
char *gta_error_msg[] = {
   "Successful completion",
   "Host not found",
   "Could not connect to host",
   "Conversation termainated abnormally",
   "Could not create output file %s",
   "Could not launch TRAFFICOP",
   "Could not lock sequence file",
   "Could not find sequence file",
   "Could not create queue file",
   "Service WDOCGEN not found",
   "Queue environment variable not found",
   "Could not create user directory on remote",
   "Branch not found on /etc/hosts",
   "Local software release too old",
   "Remote software release too old",
   "Could not change remote directory",
   "Could not transmit files",
   "Could not retrieve files",
   "Invalid genserver operation"
};

void ap_error_log(char *, ...);

#endif
