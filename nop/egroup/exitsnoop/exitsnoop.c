// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "exitsnoop.skel.h"

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

static void bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new))
    {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    struct exitsnoop_bpf *skel;
    int pid = 0, tgid = 0, child = 0, allret = 0, keep = 0, reset = 0;
    unsigned long cgid = 0;
    unsigned long ts = 100000000; // 100 milliseconds
    char msg[128] = {0};
    int err, i;

    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();

    skel = exitsnoop_bpf__open();
    if (!skel)
    {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = exitsnoop_bpf__load(skel);
    if (err)
    {
        fprintf(stderr, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }

    err = exitsnoop_bpf__attach(skel);
    if (err)
    {
        fprintf(stderr, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    printf("%s\n", msg);

    if (keep > 0)
    {
        int i;

        for (i = 0; i < skel->skeleton->prog_cnt; i++)
        {
            char buf[128] = {0};

            snprintf(buf, sizeof(buf), "/sys/fs/bpf/sched_%s",
                     skel->skeleton->progs[i].name);

            err = bpf_link__pin(*skel->skeleton->progs[i].link, buf);
            if (err)
                goto cleanup;
        }

        return 0;
    }
    else
    {
        for (;;)
            sleep(1);
    }

cleanup:
    exitsnoop_bpf__destroy(skel);
    if (child)
        wait(NULL);
    return -err;

    // usage:
    //     fprintf(stderr,
    //             "Usage: %s\n"
    //             "\tcmd, -c <cmd args>: execute command <cmd> and prioritize it\n"
    //             "\tpid, -p <pid>: prioritize task with pid <pid>\n"
    //             "\ttgid, -t <tgid>: prioritize task(s) with tgid <tgid>\n"
    //             "\tcgroup, -g <path/cgid>: prioritize task(s) within cgroup with <path/cgid>\n"
    //             "\tall, -a <ret>: suppress all non-voluntary context switches\n"
    //             "\tts, -s <timeslice>: max timeslice in milliseconds [1..1000]\n"
    //             "\tkeep, -k: keep programs loaded and attached using bpffs\n"
    //             "\treset, -r: delete all sched_ programs from bpffs\n"
    //             "\thelp, -h, -?: print this message\n",
    //             argv[0]);
    return 1;
}
