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

unsigned long pid1 = 0, pid2 = 0;
unsigned long last_time = 0;
unsigned long elapsed = 0;
unsigned long cnt = 1;
unsigned long cnt_throtl = 1;
unsigned long release_signal = 0;
int biocnt=0;

#define INVALID_RET ((unsigned long)-1L)

#define debug(args...) bpf_printk(args)

#define REQ_OP_MASK ((1 << 8) - 1)
#define bio_op(bio) \
	((bio)->bi_opf & REQ_OP_MASK)

bool op_is_write(unsigned int op)
{
	return (op & 1);
}

SEC("sched/blk_check_throttle")
int BPF_PROG(throttle, struct bio *bio, struct throtl_grp *tg)
{
	char name[100];
	int pid = 0;
	unsigned long now = 0;
	unsigned long sum = 0;
	// bpf_get_current_comm(name, 100);
	pid = bpf_get_current_pid_tgid();
	now = bpf_ktime_get_ns();
	if (last_time == 0)
		last_time = now;

	struct throtl_qnode *qn = &tg->qnode_on_self[1];
	if (!qn)
		return 0;

	struct throtl_service_queue *sq = &tg->service_queue;
	struct throtl_data *td = tg->td;
	// debug("op:%c", op_is_write(bio_op(bio)) ? 'w' : 'r');
	// debug("bi_size:%u", bio->bi_iter.bi_size);
	// struct bio* tempbio;
	// if(!bio->bi_next){
	// 		debug("bi_next is null!!!");
	// 	}
	// for(int i=0;i<10&&bio->bi_next;i++){
		
	// 	tempbio=bio->bi_next;
	// 	bio=tempbio;
	// 	debug("bio num:%d",biocnt++);
	// }
	// debug("nr_queued:%u", sq->nr_queued[1]);
	// debug("bps: %lu",tg->bps[3]);
	// debug("huancunle %u bio",sq->nr_queued[1]);
	// debug("last_bytes_disp %lu ",tg->last_bytes_disp[1]);
	// debug("slice_start %lu bio",tg->slice_start[1]);
	// debug("slice_end %lu bio",tg->slice_end[1]);
	// debug("td queuenr:%u", td->nr_queued[1]);
	// debug("bpf here");
	// if(!tg->td->dispatch_work.func)
	// debug("func is null,need init");
	// else
	// debug("func pointer:%lu",tg->td->dispatch_work.func);
	if (release_signal)
	{
		debug("queuework release!");
		release_signal = 0;
		// return 5;
	}
	if (pid == pid1)
	{
		debug("block pid %d, bio num %d", pid, cnt_throtl++);
		if(cnt++%2==0)
			return 6;
		debug("queue work!");
		return 5;
	}

	return 0;

	// debug("pid:%d", pid);
	// debug("op:%c", op_is_write(bio_op(bio)) ? 'w' : 'r');
	// debug("bi_size:%u", bio->bi_iter.bi_size);
}
