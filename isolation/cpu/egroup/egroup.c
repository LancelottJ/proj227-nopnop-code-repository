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
#include <bpf/bpf.h>
#include "egroup.skel.h"
#include "egroup.h"

struct egroup_bpf *skel;

int msgid = 2;
int loopflag = 1;

struct egrpinfo
{
	unsigned long num;
	unsigned long share;
};

/* ctrl c */
void sig_handler(int signo)
{
	msgctl(msgid, IPC_RMID, NULL);
	loopflag = 0;
	egroup_bpf__destroy(skel);
	printf("EXIT SAFELY\n");
	exit(0);
}

int print_usage_description()
{
	fprintf(stderr,
			"===============================================================================\n"
			"Usage of eBPF isolation: \n"
			"\taffinity, -A { <pid> <cpu> | ... }: set process/thread run on the given <cpu>\n"
			"\tegroup, -e { <container pid> <share> | ... }:\n"
			"\t\tset <container pid> to the egroup, isolating with given <share>\n"
			"\tmsgq-egroup, -m: waiting crun pass the message using msgq, should be added in the last\n"
			"\tcmd, -c <cmd args>: execute command <cmd> and prioritize it\n"
			"\tpid, -p <pid>: prioritize task with pid <pid>\n"
			"\ttgid, -t <tgid>: prioritize task(s) with tgid <tgid>\n"
			"\tkeep, -k: keep programs loaded and attached using bpffs\n"
			"\thelp, -h, -?: print this message\n"
			"===============================================================================\n");
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
int find_child_proc(int pid, int *arr)
{
	char path[32] = {0};
	char buf[256] = {0};
	char *temp;
	int i = 0;
	sprintf(path, "/proc/%d/task/%d/children", pid, pid);

	int fd = open(path, O_RDWR);
	if (fd < 0)
	{
		while((fd = open(path, O_RDWR)) <0 ){
			printf("waiting open...");
			sleep(1);
		}
	}
	int ret = read(fd, buf, 256);
	if(ret<0){
		perror("read child");
		return -1;
	}
	else if (ret==0){
		while(!read(fd, buf, 256)){
			printf("waiting msg...");
			sleep(2);
		}
	}

	// printf("before strtok buf:%s\n", buf);
	temp = strtok(buf, " ");
	while (temp)
	{
		// printf("%s\t", temp);
		arr[i++] = atoi(temp);
		temp = strtok(NULL, " ");
	}
	// printf("\nchild proc: %s\n", buf);
	if (ret < 0)
	{
		perror("read child");
		return -1;
	}
	close(fd);
	return 0;
}

int main(int argc, char **argv)
{

	signal(SIGINT, sig_handler);

	int pid = 0, tgid = 0, child = 0, allret = 0, keep = 0, reset = 0;
	char msg[128] = {0};
	int err, i;

	if (argc == 1)
		print_usage_description();

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
		printf("enter\n\n");
		if (!strcmp(argv[i], "help") || !strcmp(argv[i], "--help") ||
			!strcmp(argv[i], "-help") || !strcmp(argv[i], "-h") ||
			!strcmp(argv[i], "?"))
			print_usage_description();
		else if (!strcmp(argv[i], "-e"))
		{
			i++;
			int childprocarr[32] = {0};
			int updtret, lkupret = 1;
			struct egrpinfo egroupinfo;
			int ptoe_fd = bpf_object__find_map_fd_by_name(skel->obj, "pid_to_egid"),
				etoinfo_fd = bpf_object__find_map_fd_by_name(skel->obj, "egid_to_grpinfo");
			for(;i<argc;){
				
				int parent = atoi(argv[i++]);
				int share = atoi(argv[i++]);
				//printf("parent:%d \t share: %d",parent,share);
				memset(childprocarr, 0, sizeof(childprocarr));
				if(find_child_proc(parent, childprocarr)==-1){
					goto cleanup;
				}
				for (int j = 0; childprocarr[j] != 0; j++)
				{
					updtret = bpf_map_update_elem(ptoe_fd, &childprocarr[j], &parent, BPF_ANY);
					if (updtret)
						printf("wrong map update: %s\n", strerror(errno));
					lkupret = bpf_map_lookup_elem(etoinfo_fd, &parent, &egroupinfo);
					if (lkupret)
					{
						egroupinfo.num = 1;
						skel->bss->grp_shares_sum += share;
						printf("创建egroup组...\n");
					}
					else
						egroupinfo.num++;
					egroupinfo.share = share;
					bpf_map_update_elem(etoinfo_fd, &parent, &egroupinfo, BPF_ANY);
					printf("添加子进程%d进入组\n", childprocarr[j]);
				}
			}
		}
		/* Esolation mode */
		else if (!strcmp(argv[i], "-m"))
		{
			int cnt = 0;
			int ptoe_fd = bpf_object__find_map_fd_by_name(skel->obj, "pid_to_egid"),
				etoinfo_fd = bpf_object__find_map_fd_by_name(skel->obj, "egid_to_grpinfo");
			struct msgbuf buf;
			struct egrpinfo egroupinfo;
			int updtret, lkupret = 1;
			skel->bss->allret = 1;
			unsigned long egid, share, egid_d = 0;
			unsigned int pid;
			int childprocarr[32] = {0};
			/* msg queue listening */
			while (loopflag)
			{
#ifdef LOCAL_TEST
				msgrcv(msgid, &buf, sizeof(data_container), MTYPE, 0);
				egid = buf.mdata.egid;
				pid = buf.mdata.pid;
				share = buf.mdata.share;
				printf("msgqrecv: egroupid-%ld\tpid-%d\tshare-%ld\n", egid, pid, share);

				updtret = bpf_map_update_elem(ptoe_fd, &pid, &egid, BPF_ANY);
				if (updtret)
					printf("wrong map update: %s\n", strerror(errno));

				lkupret = bpf_map_lookup_elem(etoinfo_fd, &egid, &egroupinfo);
				if (lkupret)
				{
					egroupinfo.num = 1;
					skel->bss->grp_shares_sum += share;
				}
				else
					egroupinfo.num++;
				egroupinfo.share = share;
				bpf_map_update_elem(etoinfo_fd, &egid, &egroupinfo, BPF_ANY);
#else
				msgrcv(msgid, &buf, sizeof(data_container), MTYPE, 0);
				egid = buf.mdata.egid;
				pid = buf.mdata.pid;
				share = buf.mdata.share;
				printf("msgqrecv: egroupid-%ld\tpid-%d\tshare-%ld\n", egid_d, pid, share);

				/* find child proc */
				memset(childprocarr, 0, sizeof(childprocarr));
				if(find_child_proc(pid, childprocarr)==-1){
					goto cleanup;
				}
				for (int j = 0; childprocarr[j] != 0; j++)
				{
					updtret = bpf_map_update_elem(ptoe_fd, &childprocarr[j], &egid_d, BPF_ANY);
					if (updtret)
						printf("wrong map update: %s\n", strerror(errno));
					lkupret = bpf_map_lookup_elem(etoinfo_fd, &egid_d, &egroupinfo);
					if (lkupret)
					{
						egroupinfo.num = 1;
						skel->bss->grp_shares_sum += share;
					}
					else
						egroupinfo.num++;
					egroupinfo.share = share;
					bpf_map_update_elem(etoinfo_fd, &egid_d, &egroupinfo, BPF_ANY);
					printf("childprocarr: %d\t", childprocarr[j]);
				}
				printf("\negid_d:%ld\n", egid_d++);
#endif

				// 什么地方释放shares_sum

				// bpf_map_lookup_elem(skel->maps.pid_to_egid2, &pid, &egid);
				//  if (cnt++ == 0)
				//  	skel->bss->first_pid = pid;
				//  printf("pid-first_pid:%d\n", pid - skel->bss->first_pid);
				/*set egroupid to pid , modify the egroup shares*/
				// skel->bss->pid_to_egid[pid - skel->bss->first_pid] = egid;
				// skel->data->egid_to_grpinfo[egid][1]++;
				// skel->data->egid_to_grpinfo[egid][2] = skel->data->egid_to_grpinfo[egid][0] / skel->data->egid_to_grpinfo[egid][1]; // skel->bss->grp_shares_sum+=grp_shares;
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
		else if (!strcmp(argv[i], "pin") || !strcmp(argv[i], "-P"))
		{
			i++;
			err = bpf_object__pin(skel->obj, "/sys/fs/bpf/sched_egroup");
			if (err)
			{
				printf("pin gg\n");
				goto cleanup;
			}
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
