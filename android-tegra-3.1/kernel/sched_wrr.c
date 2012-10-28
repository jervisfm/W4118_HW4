/*
 * sched_wrr.c
 *
 *  Created on: Oct 28, 2012
 *      Author: w4118
 */

/*
 * Structure of file based on sched_idletask.c
 * implmementation of an idle task scheduler.
 *
 * Methods not currently implemented with functionality.
 *
 * Note: This first will attempt to add schedule for case of 1-CPU, and then
 * later on add support for SMP. This will require additional definition
 * and  implementation SMP methods declared in sched_class in sched.h
 */

/*
 * Main TODO:
 * 1) Make this the default scheduling policy for init and all of its descendants.
 * 2) Support Multiprocessor systems, like the Nexus 7.
 * 3) If the weight of a task currently on a CPU is changed, it should finish
 *    its time quantum as it was before the weight change. i.e. increasing
 *    the weight of a task currently on a CPU does not extend its current time
 *    quantum.
 * 4) When deciding which CPU a job should be assigned to, it should be
 *    assigned to the CPU with the smallest total weight (i.e. sum of the
 *    weights of the jobs on the CPU's run queue).
 * 5) Add periodic load balancing every 500ms
 */


#ifdef CONFIG_SMP
static int
select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	return task_cpu(p); /* wrr tasks as never migrated */
}
#endif /* CONFIG_SMP */
/*
 * Idle tasks are unconditionally rescheduled:
 */
static void check_preempt_curr_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	/* TO be Implemented. */

	/* Idle task impl:
	 * resched_task(rq->wrr);
	 */

}

static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	/* TO be Implemented. */
	/*
	if (printk_ratelimit(  ))
		printk("We were called to schedule a task but we have not"
			"implemnetedthe schedule yet!\n");
	*/
	return NULL;

	/** IDLE Task Impl
	 * schedstat_inc(rq, sched_gowrr);
	 * calc_load_account_wrr(rq);
	 */
}

/*
 * It is not legal to sleep in the idle task - print a warning
 * message if some code attempts to do it:
 */
static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	return ;
	/* Idle task iMPLM
	raw_spin_unlock_irq(&rq->lock);
	printk(KERN_ERR "bad: scheduling from the wrr thread!\n");
	dump_stack();
	raw_spin_lock_irq(&rq->lock);
	*/
}

/*
 * Called when a task enters a runnable state.
 * It puts the scheduling entity (task) into the red-black tree and
 * increments the nr_running variable.
 */
static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{

}

static void yield_task_wrr (struct rq *rq)
{
	/* To be implemented */
}

static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{
	/* To be implemented */
}

static void task_tick_wrr(struct rq *rq, struct task_struct *curr, int queued)
{
	/* To be implemented */
}

static void set_curr_task_wrr(struct rq *rq)
{
	/* To be implemented */
}

static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
	/* To be implemented */

}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
	/* To be implemented */

}

static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	/* To be implemented */
	return 0;
}

/*
 * Simple, special scheduling class for the per-CPU wrr tasks:
 */
static const struct sched_class wrr_sched_class = {
	/* .next is Fair Scheduler class scheduler */
	.next			= &fair_sched_class,

	/* no enqueue/yield_task for idle tasks */
	/* TODO: Add enqueue/yield task for WRR */

	.enqueue_task 		= enqueue_task_wrr,

	.yield_task		= yield_task_wrr,

	.dequeue_task		= dequeue_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};
