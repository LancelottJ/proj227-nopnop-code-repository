#include "egroup.h"

int main(int argc, char **argv)
{
    key_t key;
    int msgid;
    struct msgbuf buf;
    char pidbuf[1024];
    unsigned long share, egid = 0;
    unsigned int pid, pidnum;

    key = ftok(MSG_PATH, 0);
    if (key < 0)
    {
        ERR_EXIT("ftok error");
    }

    msgid = msgget(key, 0644 | IPC_CREAT);
    if (msgid < 0)
    {
        ERR_EXIT("msgget error");
    }
    printf("msgid: %d\n", msgid);

    // for (int i = 1; i < argc;)
    // {

    //     buf.mtype = MTYPE;
    //     buf.mdata.egid = atoi(argv[i++]);
    //     buf.mdata.pid = atoi(argv[i++]);
    //     printf("egid: %ld, pid: %d\n", buf.mdata.egid, buf.mdata.pid);
    //     msgsnd(msgid, &buf, sizeof(buf) - sizeof(long), 0);
    // }
#ifdef FORK_RUN_TEST
    FILE *fp = fopen(INPUTFILE, "r");
    if (!fp)
        perror("fopen wrong");
    while (fscanf(fp, "%ld%d", &share, &pidnum) != EOF)
    {
        // if transfer data end
        // if (fscanf(fp, "%ld%d%ld", &buf.mdata.egid, &buf.mdata.pid, &buf.mdata.shares) == EOF)
        //     return 0;

        for (int i = 0; i < pidnum; i++)
        {
            fscanf(fp, "%d", &pid);
            struct msgbuf buf = {MTYPE, {egid, pid, share}};
            printf("egid:%ld,pid:%d,shares: %ld\n", egid, pid, share);
            msgsnd(msgid, &buf, sizeof(buf) - sizeof(long), 0);
        }
        egid++;
    }
#endif
    for (int egidi = 0; egidi < 10; egidi++)
    {
        for (int pidi = 0; pidi < 1000; pidi++)
        {
            struct msgbuf buf;
            buf.mtype = MTYPE;
            buf.mdata.egid = egidi;
            buf.mdata.pid = pidi;
            msgsnd(msgid, &buf, sizeof(buf) - sizeof(long), 0);
        }

        printf("%d\n", egidi);
    }
    return 0;
}
