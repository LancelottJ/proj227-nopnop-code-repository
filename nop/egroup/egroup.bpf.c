// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

unsigned long tgidpid = 0;
unsigned long cgid = 0;
unsigned long allret = 0;
unsigned long max_exec_slice = 0;

unsigned first_pid = 0;
unsigned long grp_shares_sum = 2048;
unsigned long pid_to_egid[5] = {0};
unsigned long egid_to_grpinfo[3][3] = {{1024, 0, 0}, {1024, 0, 0}, {1024, 0, 0}};

#define INVALID_RET ((unsigned long)-1L)

#define debug(args...) bpf_printk(args)
// #define debug(args...)

struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} rb SEC(".maps");

SEC("sched/cfs_check_preempt_tick")
int BPF_PROG(tick, struct sched_entity *curr, unsigned long delta_exec)
{
	unsigned long tgidpid1;
	int ret = 0;
	unsigned int pid;
	unsigned long egid = 0;
	unsigned long weight_amplified = 0;

	/* if run too long */
	if (delta_exec > max_exec_slice)
	{
		debug("TICK:out of bound!");
		debug("curr:%d exec_time:%lld ",
			  bpf_sched_entity_to_tgidpid(curr) & 0xFFFFFFFF, delta_exec);
		return 0;
	}

	if (allret)
		return allret;

	if (curr == NULL)
		return 0;
	/* get pid from curr */
	tgidpid1 = bpf_sched_entity_to_tgidpid(curr);
	pid = tgidpid1 & 0xFFFFFFFF;
	unsigned int pos = pid - first_pid;
	if (pos > 4 || pos < 0)
		return 0;
	/* get egroup id*/
	egid = pid_to_egid[pos];
	if (egid != 0)
	{
		if (egid < 0 || egid > 2)
			return 0;
		/* get the inverse ratio */
		weight_amplified = grp_shares_sum / egid_to_grpinfo[egid][2];
		/* for higher accuracy */
		ret = weight_amplified * 1024;

		debug("TICK=======");
		if (ret)
		{
			debug("delta:%ld", delta_exec);
			debug("curr:%6d  xxdiff_start%ld", tgidpid1 & 0xFFFFFFFF, curr->exec_start - prevstart);
			prevstart = curr->exec_start;
			debug("curr:%6d  xxsum_time:%ld", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime);
			debug("curr:%6d  xxdiffprev_time:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->prev_sum_exec_runtime - prevprev);
			debug("curr:%6d  sum-prev:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime - curr->prev_sum_exec_runtime);

			debug("curr:%6d  xxdelta_time:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime - curr->prev_sum_exec_runtime);
			// debug("Return:  %2d  xxvruntime:%lld",ret, curr->vruntime-(curr->sum_exec_runtime-curr->prev_sum_exec_runtime));
			debug("Return:  %2d  xxdiff_vruntime:%lld", ret, curr->vruntime - prevvruntime);
			prevvruntime = curr->vruntime;
		}
	}
	/* cgroup mode */
	else if (cgid)
	{
		if (bpf_sched_entity_belongs_to_cgrp(curr, cgid))
		{
			ret = -1;
			debug("tick cg %lu %d", bpf_sched_entity_to_cgrpid(curr), ret);
		}
	}

	return ret;
}
SEC("sched/cfs_check_preempt_wakeup")
int BPF_PROG(wakeup, struct task_struct *curr, struct task_struct *p)
{
	unsigned long tgidpid1, tgidpid2;
	int ret = 0;

	if (allret)
		return allret;

	tgidpid1 = bpf_sched_entity_to_tgidpid(curr);
	tgidpid2 = bpf_sched_entity_to_tgidpid(se);
	pid = tgidpid1 & 0xFFFFFFFF;
	unsigned int pos = pid - first_pid;
	if (pos > 4 || pos < 0)
		return 0;

	egid = pid_to_egid[pos];
	if (egid < 0 || egid > 2)
		return 0;

	unsigned long eg_member = ++egid_to_grpinfo[egid][1];
	if (eg_member != 0)
	{
		egid_to_grpinfo[egid][2] = egid_to_grpinfo[egid][0] / eg_member;
		// bpf_printk("egpb:%ld", egid_to_grpinfo[egid][2]);
	}

	debug("wake=======");
	if (ret)
	{
		debug("wakeup1 tgid %d pid %d", tgidpid1 >> 32,
			  tgidpid1 & 0xFFFFFFFF);
		debug("wakeup2 tgid %d pid %d", tgidpid2 >> 32,
			  tgidpid2 & 0xFFFFFFFF);
		debug("wakeup ret %d", ret);
	}

	return ret;
}

SEC("tp/sched/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx)
{
	struct task_struct *task;
	struct event *e;
	unsigned int pid = 0, tid;
	u64 id = 0, ts, *start_ts, duration_ns = 0;
	unsigned long egid = 0;

	/* get PID and TID of exiting thread/process */
	id = bpf_get_current_pid_tgid();
	pid = id >> 32;
	tid = (u32)id;

	/* ignore thread exits */
	if (pid != tid)
		return 0;

	unsigned int pos = pid - first_pid;
	if (pos > 4 || pos < 0)
		return 0;

	egid = pid_to_egid[pos];
	if (egid < 0 || egid > 2)
		return 0;

	unsigned long eg_member = --egid_to_grpinfo[egid][1];
	if (eg_member != 0)
	{
		egid_to_grpinfo[egid][2] = egid_to_grpinfo[egid][0] / eg_member;
		// bpf_printk("egpb:%ld", egid_to_grpinfo[egid][2]);
	}

	/* reserve sample from BPF ringbuf */
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if (!e)
		return 0;

	/* fill out the sample with data */
	task = (struct task_struct *)bpf_get_current_task();

	e->duration_ns = duration_ns;
	e->pid = pid;
	e->ppid = BPF_CORE_READ(task, real_parent, tgid);
	e->exit_code = (BPF_CORE_READ(task, exit_code) >> 8) & 0xff;
	bpf_get_current_comm(&e->comm, sizeof(e->comm));

	/* send data to user-space for post-processing */
	bpf_ringbuf_submit(e, 0);
	return 0;
}