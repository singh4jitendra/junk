#ifndef _BDB_H_
#define _BDB_H_

#define BRANCHNUMBER(x) (((x)[0] - '0') * 1000) + \
                        (((x)[1] - '0') * 100) + \
                        (((x)[2] - '0') * 10) + \
                        ((x)[3] - '0')

#define BRANCHKEY    (('W' << 8) + 1)
#define PROCESSKEY   (('W' << 8) + 2)

typedef struct BRANCHTABLE {
    int branch;
    pid_t pid;
    unsigned long connections;
} BRANCHTABLE;

typedef struct PROCESSTABLE {
    pid_t pid;
    unsigned long connections;
    char fifo[50];
    int debugFile;
    int ready;
} PROCESSTABLE;

#endif
