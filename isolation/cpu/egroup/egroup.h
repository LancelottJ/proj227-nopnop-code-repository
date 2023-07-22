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
    unsigned long share;
} data_container;

struct msgbuf
{
    long mtype;
    data_container mdata;
};

#define MTYPE 1
#define MSG_PATH "/home/femu/proj227-nopnop-code-repository/isolation/cpu/egroup/.msgq"
#define INPUTFILE "/home/femu/proj227-nopnop-code-repository/isolation/cpu/egroup/output"
#define ERR_EXIT(m) \
    perror(m);      \
    exit(-1);

#endif