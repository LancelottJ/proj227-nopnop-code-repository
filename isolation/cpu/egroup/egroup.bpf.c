// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct egrpinfo
{
	unsigned long num;
	unsigned long share;
};
// #define TASK_COMM_LEN 16
// #define MAX_FILENAME_LEN 127
// struct event
// {
//     int pid;
//     int ppid;
//     unsigned exit_code;
//     unsigned long long duration_ns;
//     char comm[TASK_COMM_LEN];
// };
unsigned long tgidpid = 0;
unsigned long allret = 0,alret=0;
unsigned long max_exec_slice = 0;

struct str
{
	char a[20];
};

unsigned long grp_shares_sum = 0;
#define INVALID_RET ((unsigned long)-1L)

#define debug(args...) bpf_printk(args)
// #define debug(args...)

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, pid_t);
	__type(value, u64);
} pid_to_egid SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u64);
	__type(value, struct egrpinfo);
} egid_to_grpinfo SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, struct str);
	__uint(max_entries, 256);
} test_sharedmap SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, pid_t);
	__type(value, u64);
} pid_to_wkupcnt SEC(".maps");

struct
{
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} rb SEC(".maps");

SEC("sched/cfs_check_update_curr")
int BPF_PROG(tick, struct sched_entity *curr, unsigned long delta_exec)
{
	unsigned long tgidpid1 = bpf_sched_entity_to_tgidpid(curr);
	int pid = tgidpid1 & 0xFFFFFFFF, ret = 0;
	unsigned long weight_amplified = 0, thread_share;

	unsigned long *egid;
	struct egrpinfo *egroupinfo;

	// if (allret)
	// 	return ret;

	if (pid == -1 || !curr)
		return ret;
	/* if run too long */
	// if (delta_exec > max_exec_slice)
	// {
	// 	debug("TICK:out of bound!");
	// 	debug("curr:%d exec_time:%lld ",
	// 		  bpf_sched_entity_to_tgidpid(curr) & 0xFFFFFFFF, delta_exec);
	// 	return 0;
	// }
	egid = bpf_map_lookup_elem(&pid_to_egid, &pid);
	if (!egid)
		return ret;
	egroupinfo = bpf_map_lookup_elem(&egid_to_grpinfo, egid);
	if (!egroupinfo)
		return ret;
	if (egroupinfo->num == 0)
		return ret;
	thread_share = egroupinfo->share / egroupinfo->num;
	// debug("get egrp, pid=%d, egid=%ld", pid, *egid);
	// debug("egrpinfo: num-%lu\tshare-%lu\t", egroupinfo->num, egroupinfo->share);

	/* get the inverse ratio */
	weight_amplified = grp_shares_sum / thread_share;
	/* for higher accuracy */
	ret = weight_amplified * 1024;

	// debug("TICK=======");
	// if (ret)
	// {
	// 	debug("delta:%ld", delta_exec);
	// 	debug("curr:%6d  xxdiff_start%ld", tgidpid1 & 0xFFFFFFFF, curr->exec_start - prevstart);
	// 	prevstart = curr->exec_start;
	// 	debug("curr:%6d  xxsum_time:%ld", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime);
	// 	debug("curr:%6d  xxdiffprev_time:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->prev_sum_exec_runtime - prevprev);
	// 	debug("curr:%6d  sum-prev:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime - curr->prev_sum_exec_runtime);

	// 	debug("curr:%6d  xxdelta_time:%ld\n", tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime - curr->prev_sum_exec_runtime);
	// 	// debug("Return:  %2d  xxvruntime:%lld",ret, curr->vruntime-(curr->sum_exec_runtime-curr->prev_sum_exec_runtime));
	// 	debug("Return:  %2d  xxdiff_vruntime:%lld", ret, curr->vruntime - prevvruntime);
	// 	prevvruntime = curr->vruntime;
	// }
	return ret;
}

SEC("tp/sched/sched_process_exit")
int handle_exit(struct trace_event_raw_sched_process_template *ctx)
{
	struct task_struct *task;
	struct event *e;
	unsigned int pid = 0, tgid;
	unsigned long tgidpid = 0, *egid;
	struct egrpinfo *egroupinfo;
	int ret = 0, delret = 1;

	// if (allret)
	// 	return ret;

	/* get PID and TID of exiting thread/process */
	tgidpid = bpf_get_current_pid_tgid();
	tgid = tgidpid >> 32;
	pid = (u32)tgidpid;

	/* ignore user_thread exits */
	if (pid != tgid)
		return 0;

	egid = bpf_map_lookup_elem(&pid_to_egid, &pid);
	if (!egid)
		return ret;
	egroupinfo = bpf_map_lookup_elem(&egid_to_grpinfo, egid);
	if (!egroupinfo)
		return ret;

	/* free the pid*/
	delret = bpf_map_delete_elem(&pid_to_egid, &pid);
	if (delret)
		debug("wrong map delete");
	debug("free thread %d in egroup: %d", pid, *egid);

	/* free the egid */
	egroupinfo->num--;
	if (egroupinfo->num <= 0)
	{
		grp_shares_sum -= egroupinfo->share;
		delret = bpf_map_delete_elem(&egid_to_grpinfo, egid);
		if (delret)
			debug("wrong map delete");
		debug("free egroup: %d", *egid);
		return ret;
	}

	// /* reserve sample from BPF ringbuf */
	// e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	// if (!e)
	// 	return 0;

	// /* fill out the sample with data */
	// task = (struct task_struct *)bpf_get_current_task();

	// e->duration_ns = duration_ns;
	// e->pid = pid;
	// e->ppid = BPF_CORE_READ(task, real_parent, tgid);
	// e->exit_code = (BPF_CORE_READ(task, exit_code) >> 8) & 0xff;
	// bpf_get_current_comm(&e->comm, sizeof(e->comm));

	// /* send data to user-space for post-processing */
	// bpf_ringbuf_submit(e, 0);
	return 0;
}

SEC("sched/cfs_check_block_entity")
int BPF_PROG(block, struct task_struct *curr, unsigned int op)
{
	
	unsigned long *egid,pid=curr->pid;
	struct egrpinfo *egroupinfo;
	int ret,delret=1;
	unsigned long wakeupcnt=1,updtret=1,*wkupcnt_ptr;

	if (allret)
		return ret;
	


	if (!curr)
		return ret;
	egid = bpf_map_lookup_elem(&pid_to_egid, &pid);
	if (!egid)
		return ret;
	egroupinfo = bpf_map_lookup_elem(&egid_to_grpinfo, egid);
	if (!egroupinfo)
		return ret;
	if (egroupinfo->num == 0)
		return ret;
	// delret = bpf_map_delete_elem(&pid_to_egid, &pid);
	// if (delret)
	// 	debug("wrong map delete");
	
	
	
	/* free the pid */
	egroupinfo->num--;
	updtret = bpf_map_update_elem(&pid_to_wkupcnt, &pid,&wakeupcnt,BPF_ANY);
	if(updtret)
		debug("update map error");


	debug("block thread %d in egroup: %d, egroupnum:%d", pid, *egid,egroupinfo->num);
	
	return 0;
}

SEC("tp/sched/sched_wakeup")
int handle_wakeup(struct trace_event_raw_sched_process_template *ctx)
{
	struct task_struct *task;
	unsigned int pid = 0, tgid;
	unsigned long tgidpid = 0, *egid,*wkupcnt;
	struct egrpinfo *egroupinfo;
	int ret = 0, updtret = 1,delret=0;

	if (allret)
		return ret;

	/* get PID and TID of exiting thread/process */
	tgidpid = bpf_get_current_pid_tgid();
	tgid = tgidpid >> 32;
	pid = (u32)tgidpid;

	/* ignore user_thread exits */
	if (pid != tgid)
		return ret;

	/* check if pid blocked */
	wkupcnt = bpf_map_lookup_elem(&pid_to_wkupcnt,&pid);
	if(!wkupcnt)
		return ret;
	
	/* update its block status */
	delret = bpf_map_delete_elem(&pid_to_wkupcnt, &pid);
	if(delret)
		return ret;
	egid = bpf_map_lookup_elem(&pid_to_egid, &pid);
	if (!egid)
		return ret;
	egroupinfo = bpf_map_lookup_elem(&egid_to_grpinfo, egid);
	if (!egroupinfo)
		return ret;
	if (egroupinfo->num < 0)
		return ret;
		
	egroupinfo->num++;
	debug("wakeup thread %d in egroup: %d, egroupnum:%d", pid, *egid,egroupinfo->num);
	
	return 0;
}
