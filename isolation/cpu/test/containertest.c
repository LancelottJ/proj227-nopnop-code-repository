#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main()
{
    pid_t pid;
    int status;
    cpu_set_t set;
    CPU_SET(0, &set);
    for (int i = 0; i < 4; i++)
    {
        pid = fork();
        if (pid < 0)
            perror("fork");
        else if (pid == 0)
        {
            // printf("childtid:%d\n", gettid());
            // printf("child pid:%d\n", getpid());
            if(sched_setaffinity(getpid(), sizeof(set),&set)==-1){
                perror("set_affinity:");
                return 0;
            }
            int a = 0;
            for (int j = 0; j < 10; j++){
                for (int k = 0; k < (i==2?500000000:1000000000); k++)
                    a += j;
                if(i==3)
                    sleep(10);   
            }
            printf("child thread %d done!\n", i);
            exit(100);
        }
        else
        {
            // parent
            continue;
        }
    }

    while ((pid = wait(&status)) > 0)
        ;
    printf("parent done!\n");
    return 0;
}