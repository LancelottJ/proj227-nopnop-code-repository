// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <stdlib.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

unsigned long tgidpid = 0;
unsigned long cgid = 0;
unsigned long allret = 0;
unsigned long max_exec_slice = 0;
unsigned long egroup[10][1024]={0};
unsigned long prevvruntime=0;
unsigned long prevstart=0;
unsigned long prevprev=0;

#define INVALID_RET ((unsigned long)-1L)

#define debug(args...) bpf_printk(args)
// #define debug(args...)


SEC("sched/cfs_check_preempt_wakeup")
int BPF_PROG(wakeup, struct task_struct *curr, struct task_struct *p)
{
	unsigned long tgidpid1, tgidpid2;
	int ret = 0;

	if (allret)
		return allret;

	if (tgidpid)
	{
		tgidpid1 = (unsigned long)curr->tgid << 32 | curr->pid;
		tgidpid2 = (unsigned long)p->tgid << 32 | p->pid;

		// if ((tgidpid1 & tgidpid) == tgidpid)
		if (tgidpid1 == tgidpid){
			//ret = -1;
		}
		// else if ((tgidpid2 & tgidpid) == tgidpid)
		else if (tgidpid2 == tgidpid){
			//ret = 1;
		}
		
		
			debug("wakeup1 tgid %d pid %d", tgidpid1 >> 32,
				  tgidpid1 & 0xFFFFFFFF);
			debug("wakeup2 tgid %d pid %d", tgidpid2 >> 32,
				  tgidpid2 & 0xFFFFFFFF);
			debug("wakeup ret %d", ret);
		
	}
	else if (cgid)
	{
		if (bpf_sched_entity_belongs_to_cgrp(&curr->se, cgid))
			ret = -1;
		else if (bpf_sched_entity_belongs_to_cgrp(&p->se, cgid))
			ret = 1;

		if (ret)
		{
			tgidpid1 = (unsigned long)curr->tgid << 32 | curr->pid;
			tgidpid2 = (unsigned long)p->tgid << 32 | p->pid;

			// debug("vruntime1:%lld", curr->se->vruntime);
			// debug("vruntime2:%lld", p->se->vruntime);
			debug("wakeup1 tgid %d pid %d", tgidpid1 >> 32,
				  tgidpid1 & 0xFFFFFFFF);
			debug("wakeup2 tgid %d pid %d", tgidpid2 >> 32,
				  tgidpid2 & 0xFFFFFFFF);
			debug("wakeup ret %d", ret);
		}
	}

	return ret;
}

SEC("sched/cfs_wakeup_preempt_entity")
int BPF_PROG(preempt_entity, struct sched_entity *curr, struct sched_entity *se)
{
	int ret = 0;
	debug("preempt enter");
	if (allret)
		return allret;

	if (curr == NULL || se == NULL){
		debug("NULL");
		if(curr==NULL) 
		debug("preempt curr is NULL");
		else 
		debug("preempt se is NULL");
		return 0;
	}
	/* pid/tgid mode */
	if (tgidpid)
	{
		unsigned long tgidpid1, tgidpid2;

		tgidpid1 = bpf_sched_entity_to_tgidpid(curr);
		tgidpid2 = bpf_sched_entity_to_tgidpid(se);

		// if ((tgidpid1 & tgidpid) == tgidpid)
		if (tgidpid1 == tgidpid){
			//ret = -1;
		}
		// else if ((tgidpid2 & tgidpid) == tgidpid)
		else if (tgidpid2 == tgidpid){
			//ret = 1;
		}
		//if (ret)
		//{
			//			debug("entity1 tgid %d pid %d", tgidpid1 >> 32,
			//				  tgidpid1 & 0xFFFFFFFF);
			//			debug("entity2 tgid %d pid %d", tgidpid2 >> 32,
			//				  tgidpid2 & 0xFFFFFFFF);
			//			debug("entity ret %d", ret);
			debug("1vruntime:%d=>%lld", tgidpid1 >> 32, curr->vruntime);
			debug("2vruntime:%d=>%lld", tgidpid2 >> 32, se->vruntime);
			debug("%d <====== %d", tgidpid1 >> 32, tgidpid2 >> 32);
			debug("preempt ret:%d\n", ret);
		//}

		/* cgroup id mode */
	}
	else if (cgid)
	{
		if (bpf_sched_entity_belongs_to_cgrp(curr, cgid))
			ret = 1;
		else if (bpf_sched_entity_belongs_to_cgrp(se, cgid))
			ret = -1;

		if (ret)
		{
			debug("entity cg %lu", bpf_sched_entity_to_cgrpid(curr));
			debug("entity cg %lu", bpf_sched_entity_to_cgrpid(se));
			debug("entity cg %d", ret);
		}
	}

	return ret;
}

SEC("sched/cfs_check_preempt_tick")
int BPF_PROG(tick, struct sched_entity *curr, unsigned long delta_exec)
{
	unsigned long tgidpid1;
	int ret = 0,ret1=0;;

	if (delta_exec > 50000000000)
	{
		debug("TICK:out of bound!#################################");
		debug("curr:%d exec_time:%lld ",
			  bpf_sched_entity_to_tgidpid(curr) & 0xFFFFFFFF, delta_exec);
		return 0;
	}

	if (allret)
		return allret;

	if (curr == NULL)
		return 0;

	/* pid/tgid mode */
	if (tgidpid)
	{
		tgidpid1 = bpf_sched_entity_to_tgidpid(curr);
		if(tgidpid1==-1){
			debug("notatask");
		}
		//if ((tgidpid1 & tgidpid) == tgidpid){
		if (tgidpid1 == tgidpid){
			ret = 2;
			ret1=-1;
			//curr->sum_exec_runtime-=delta_exec/2;
		}
		//			debug("tick tgid %d pid %d ret %d", tgidpid1 >> 32,
		//				  tgidpid1 & 0xFFFFFFFF, ret);
		debug("TICK=======");
		if(ret1){
		debug("delta:%ld",delta_exec);
		debug("curr:%6d  xxdiff_start%ld",tgidpid1 & 0xFFFFFFFF, curr->exec_start-prevstart);
		prevstart=curr->exec_start;
		debug("curr:%6d  xxsum_time:%ld",tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime);
		debug("curr:%6d  xxdiffprev_time:%ld\n",tgidpid1 & 0xFFFFFFFF, curr->prev_sum_exec_runtime-prevprev);
		debug("curr:%6d  sum-prev:%ld\n",tgidpid1 & 0xFFFFFFFF,curr->sum_exec_runtime-curr->prev_sum_exec_runtime);
		
		debug("curr:%6d  xxdelta_time:%ld\n",tgidpid1 & 0xFFFFFFFF, curr->sum_exec_runtime-curr->prev_sum_exec_runtime);
		//debug("Return:  %2d  xxvruntime:%lld",ret, curr->vruntime-(curr->sum_exec_runtime-curr->prev_sum_exec_runtime));
		debug("Return:  %2d  xxdiff_vruntime:%lld",ret, curr->vruntime-prevvruntime);
		prevvruntime=curr->vruntime;
		}
		/* cgroup id mode */
	}
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
