#include "egroup.h"

int main(int argc, char **argv)
{
    key_t key;
    int msgid;
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
    printf("%d\n", msgid);

    for (int i = 1; i < argc;)
    {
        struct msgbuf buf;
        buf.mtype = MTYPE;
        buf.mdata.egid = atoi(argv[i++]);
        buf.mdata.pid = atoi(argv[i++]);
        // printf("%ld,%d\n", buf.mdata.egid, buf.mdata.pid);
        msgsnd(msgid, &buf, sizeof(buf) - sizeof(long), 0);
    }

    // for (int egidi = 0; egidi < 10; egidi++)
    //     for (int pidi = 0; pidi < 1000; pidi++)
    //     {
    //         struct msgbuf buf;
    //         buf.mtype = MTYPE;
    //         buf.mdata.egid = egidi;
    //         buf.mdata.pid = pidi;
    //         msgsnd(msgid, &buf, sizeof(buf) - sizeof(long), 0);
    //     }

    return 0;
}
