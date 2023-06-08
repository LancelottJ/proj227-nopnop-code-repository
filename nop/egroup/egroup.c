// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

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
#include "egroup.skel.h"
#include "egroup.h"

struct egroup_bpf *skel;

int msgid = 2;
int loopflag = 1;
/* ctrl c */
void sig_handler(int signo)
{
	msgctl(msgid, IPC_RMID, NULL);
	loopflag = 0;
	egroup_bpf__destroy(skel);
	printf("EXIT SAFELY\n");
	exit(0);
}

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

	signal(SIGINT, sig_handler);

	int pid = 0, tgid = 0, child = 0, allret = 0, keep = 0, reset = 0;
	unsigned long cgid = 0;
	unsigned long ts = 100000000; // 100 milliseconds
	char msg[128] = {0};
	int err, i;

	/* init msgqueue */
	key_t key;
	key = ftok(MSG_PATH, 0);
	if (key < 0)
	{
		ERR_EXIT("ftok error");
	}
	msgid = msgget(key, 0644 | IPC_CREAT);
	if (msgid < 0)
	{
		ERR_EXIT("msgrecv:msgget");
	}

	libbpf_set_print(libbpf_print_fn);
	bump_memlock_rlimit();

	/* libbpf eventually */
	skel = egroup_bpf__open();
	if (!skel)
	{
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}
	err = egroup_bpf__load(skel);
	if (err)
	{
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		goto cleanup;
	}

	err = egroup_bpf__attach(skel);
	if (err)
	{
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		goto cleanup;
	}

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "help") || !strcmp(argv[i], "--help") ||
			!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h") ||
			!strcmp(argv[i], "?"))
			goto usage;
		/*prioritize with shell command*/
		else if (!strcmp(argv[i], "cmd") || !strcmp(argv[i], "-c"))
		{
			/* end args */
			if (i++ == argc)
				goto usage;
			child = fork();
			switch (child)
			{
			case -1:
				fprintf(stderr, "Failed to fork\n");
				return -1;
			case 0:
				sleep(3);
				printf("----------------------------------------\n");
				return execvp(argv[i], &argv[i]);
			default:
				pid = child;
			}
			/* Esolation mode */
			else if (!strcmp(argv[i], "egroup"))
			{
				int cnt = 0;
				/* msg queue listening */
				while (loopflag)
				{
					struct msgbuf buf;
					msgrcv(msgid, &buf, sizeof(data_container), MTYPE, 0);
					unsigned long egid = buf.mdata.egid;
					unsigned int pid = buf.mdata.pid;
					printf("egroupid:%ld\tpid:%d\t\n", egid, pid);
					if (cnt++ == 0)
						skel->bss->first_pid = pid;
					printf("pid-first_pid:%d\n", pid - skel->bss->first_pid);
					/*set egroupid to pid , modify the egroup shares*/
					skel->bss->pid_to_egid[pid - skel->bss->first_pid] = egid;
					skel->data->egid_to_grpinfo[egid][1]++;
					skel->data->egid_to_grpinfo[egid][2] = skel->data->egid_to_grpinfo[egid][0] / skel->data->egid_to_grpinfo[egid][1]; // skel->bss->grp_shares_sum+=grp_shares;
				}
			}
			/*prioritize task(s) with pid*/
			else if (!strcmp(argv[i], "pid") || !strcmp(argv[i], "-p"))
			{
				if (i++ == argc)
					goto usage;
				pid = atoi(argv[i]);
			}
			/*prioritize task(s) with tgid*/
			else if (!strcmp(argv[i], "tgid") || !strcmp(argv[i], "-t"))
			{
				if (i++ == argc)
					goto usage;
				tgid = atoi(argv[i]);
			}
			else
			{
				goto usage;
			}
		}

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
		egroup_bpf__destroy(skel);
		if (child)
			wait(NULL);
		return -err;

	usage:
		fprintf(stderr,
				"Usage: %s\n"
				"\tcmd, -c <cmd args>: execute command <cmd> and prioritize it\n"
				"\tpid, -p <pid>: prioritize task with pid <pid>\n"
				"\ttgid, -t <tgid>: prioritize task(s) with tgid <tgid>\n"
				"\tkeep, -k: keep programs loaded and attached using bpffs\n"
				"\thelp, -h, -?: print this message\n",
				argv[0]);
		return 1;
	}
