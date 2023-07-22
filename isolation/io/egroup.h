#ifndef __EGROUP__
#define __EGROUP__

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>

typedef struct
{
    unsigned long egid;
    unsigned int pid;
} data_container;

struct msgbuf
{
    long mtype;
    data_container mdata;
};

#define TASK_COMM_LEN 16
#define MAX_FILENAME_LEN 127

struct event
{
    int pid;
    int ppid;
    unsigned exit_code;
    unsigned long long duration_ns;
    char comm[TASK_COMM_LEN];
};

#define MTYPE 1
#define MSG_PATH "./.here"
#define ERR_EXIT(m) \
    perror(m);      \
    exit(-1);

#endif