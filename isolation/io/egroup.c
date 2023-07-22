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

	int pid = 0, tgid = 0, child = 0, allret = 0, keep = 0, reset = 0;
	unsigned long cgid = 0;
	unsigned long ts = 100000000; // 100 milliseconds
	char msg[128] = {0};
	int err, i;

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

	for (;;)
	{
		unsigned long a;
		scanf("%lu", &a);
		skel->bss->pid1 = a;
		printf("%lu\n", skel->bss->pid1);

		// skel->bss->release_signal = 1;
		// sleep(1);
	}
cleanup:
	egroup_bpf__destroy(skel);
	if (child)
		wait(NULL);
	return -err;
}
