/* Forward declaration. Definition found at bottom of this file */
static const struct sched_class wrr_sched_class;
/* We use this function here. It's defined in sched.c #5109 */
static bool check_same_owner(struct task_struct *p);

/* Define general locks */
static DEFINE_SPINLOCK(SET_WEIGHT_LOCK);

#ifdef CONFIG_SMP

static void wrr_rq_load_balance(void);

/* Define locks needed for SMP case */
static DEFINE_SPINLOCK(LOAD_BALANCE_LOCK);

#endif /* CONFIG_SMP */

/* We didn't have time to rename this function. */
static enum hrtimer_restart print_current_time(struct hrtimer *timer)
{
	ktime_t period_ktime;
	struct timespec period = {
		.tv_nsec = SCHED_WRR_REBALANCE_TIME_PERIOD_NS,
		.tv_sec = 0
	};
	period_ktime = timespec_to_ktime(period);

#ifdef WRR_LOAD_BALANCE
	wrr_rq_load_balance();
#endif
	hrtimer_forward(timer, timer->base->get_time(), period_ktime);
	return HRTIMER_RESTART;
}

/* Determines if the given weight is valid.
 * i.e. within the range SCHED_WRR_MIN_WEIGHT AND
 * SCHED_WRR_MAX_WEIGHTinclusive. These are defined in sched.h file.
 * and currently are 1,20 respectively.
 * Returns 1 if true and false otherwise.  */
static int valid_weight(unsigned int weight)
{
	if (weight >= SCHED_WRR_MIN_WEIGHT && weight <= SCHED_WRR_MAX_WEIGHT)
		return 1;
	else
		return 0;
}

/* Update the time slice and time left parameters which are dervied
 * from the weight */
static void update_timings(struct task_struct *p)
{
	if (p == NULL)
		return;
	p->wrr.time_slice = p->wrr.weight * SCHED_WRR_TIME_QUANTUM;
	p->wrr.time_left = p->wrr.time_slice / SCHED_WRR_TICK_FACTOR;
}

static void update_timings_after_wt_change(struct task_struct *p)
{
	struct rq *rq;
	if (p == NULL)
		return;

	rq = task_rq(p);
	if (rq->curr == p || current == p) {
		/* Let Running Task Finish current time slice */
		p->wrr.time_slice = p->wrr.weight * SCHED_WRR_TIME_QUANTUM;
	} else {
		p->wrr.time_slice = p->wrr.weight * SCHED_WRR_TIME_QUANTUM;
		p->wrr.time_left = p->wrr.time_slice / SCHED_WRR_TICK_FACTOR;
	}
}

/* Initializes the given task which is meant to be handled/processed
 * by this scheduler */
static void init_task_wrr(struct task_struct *p)
{
	struct sched_wrr_entity *wrr_entity;
	if (p == NULL)
		return;

	wrr_entity = &p->wrr;
	wrr_entity->task = p;
	/* Use Default Parameters if the weight is still the default,
	 * or weight is invalid */
	if (wrr_entity->weight == SCHED_WRR_DEFAULT_WEIGHT ||
	    !valid_weight(wrr_entity->weight)) {

		wrr_entity->weight = SCHED_WRR_DEFAULT_WEIGHT;

		wrr_entity->time_slice =
			SCHED_WRR_DEFAULT_WEIGHT * SCHED_WRR_TIME_QUANTUM;
		wrr_entity->time_left =
			wrr_entity->time_slice / SCHED_WRR_TICK_FACTOR;
	} else { /* Use the current weight value */
		wrr_entity->time_slice =
			wrr_entity->weight * SCHED_WRR_TIME_QUANTUM;
		wrr_entity->time_left =
			wrr_entity->time_slice / SCHED_WRR_TICK_FACTOR;

	}

	/* Initialize the list head just to be safe */
	INIT_LIST_HEAD(&wrr_entity->run_list);
}

/* Return a pointer to the embedded sched_wrr_entity */
static struct sched_wrr_entity *sched_wrr_entity_of_task(struct task_struct *p)
{
	if (p == NULL)
		return NULL;

	return &p->wrr;
}

/* Return a pointer to the sched_wrr specific run queue */
static struct wrr_rq *sched_wrr_rq(struct rq *rq)
{
	if (rq == NULL)
		return NULL;
	return &rq->wrr;
}

/* Return the task_struct in which the given @wrr_entity is embedded inside */
static struct task_struct *wrr_task_of(struct sched_wrr_entity *wrr_entity)
{
	return wrr_entity->task;
}

/* Return the wrr_rq (run queue struct) in which the given enitity belongs */
static struct wrr_rq *wrr_rq_of_wrr_entity(struct sched_wrr_entity *wrr_entity)
{
	struct task_struct *p = wrr_task_of(wrr_entity);
	struct rq *rq = task_rq(p);

	return &rq->wrr;
}

/* Helper method that determines if the given entity is already
 * in the run queue. Return 1 if true and 0 if false */
static inline int on_wrr_rq(struct sched_wrr_entity *wrr_entity)
{
	/* If any item is on the wrr_rq, then the run
	 * list will NOT be empty. */
	if (list_empty(&wrr_entity->run_list))
		return 0;
	else
		return 1;
}

/* Returns the queue size given a sched_wrr_entity */
static int wrr_entity_queue_size(struct sched_wrr_entity *head_entity)
{
	struct list_head *curr;
	int counter = 0;
	struct list_head *head = &head_entity->run_list;

	if (head_entity == NULL)
		return 0;

	for (curr = head->next; curr != head; curr = curr->next)
		++counter;
	return counter;
}

/* Helper method that requeues a task */
static void requeue_task_wrr(struct rq *rq, struct task_struct *p)
{
	struct list_head *head;
	struct sched_wrr_entity *wrr_entity = sched_wrr_entity_of_task(p);
	struct wrr_rq *wrr_rq = sched_wrr_rq(rq);
	head = &wrr_rq->run_queue.run_list;

	/* Check if the task is the only one in the run-queue.
	 * In this case, we won't need to re-queue it can just
	 * leave it as it is. */
	if (wrr_rq->size == 1)
		return;


	spin_lock(&wrr_rq->wrr_rq_lock);

	/* There is more than 1 task in queue, let's move this
	 * one to the back of the queue */
	list_move_tail(&wrr_entity->run_list, head);


	spin_unlock(&wrr_rq->wrr_rq_lock);
}

/* Update the current runtime statistics. Modeled after
 * update_curr_rt in sched_rt.c  */
static void update_curr_wrr(struct rq *rq)
{
	struct task_struct *curr = rq->curr;

	u64 delta_exec;

	if (curr->sched_class != &wrr_sched_class)
		return;

	delta_exec = rq->clock_task - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->se.statistics.exec_max,
			max(curr->se.statistics.exec_max, delta_exec));

	curr->se.sum_exec_runtime += delta_exec;
	account_group_exec_runtime(curr, delta_exec);

	curr->se.exec_start = rq->clock_task;
	cpuacct_charge(curr, delta_exec);
}

/* This function chooses the most appropriate task eligible to run next.
 * This is called with Interrupts DISBALED. and us holding the rq->lock */
static struct task_struct *pick_next_task_wrr(struct rq *rq)
{
	struct task_struct *p;
	struct sched_wrr_entity *head_entity;
	struct sched_wrr_entity *next_entity;
	struct list_head *head;
	struct wrr_rq *wrr_rq =  &rq->wrr;

	/* There are no runnable tasks */
	if (rq->nr_running <= 0)
		return NULL;

	/* Pick the first element in the queue.
	 * The item will automatically be re-queued back, in task_tick
	 * funciton */
	head_entity = &wrr_rq->run_queue;
	head = &head_entity->run_list;

	next_entity = list_entry(head->next, struct sched_wrr_entity, run_list);

	p = wrr_task_of(next_entity);

	if (p == NULL)
		return p;

	p->se.exec_start = rq->clock_task;

	/* Recompute the time left + time slice value incase weight
	 * of task has been changed */

	return p;
}

/*
 * When a task is no longer runnable, this function is called.
 * We should decrements the nr_running variable in wrr_rq struct.
 */
static void
dequeue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct sched_wrr_entity *wrr_entity = &p->wrr;
	struct wrr_rq *wrr_rq = wrr_rq_of_wrr_entity(wrr_entity);

	spin_lock(&wrr_rq->wrr_rq_lock);

	update_curr_wrr(rq);
	if (!on_wrr_rq(wrr_entity)) { /* Should not happen */
		BUG();
		dump_stack();
	}

	/* Remove the task from the queue */
	list_del(&wrr_entity->run_list);

	/* update statistics counts */
	--wrr_rq->nr_running;
	--wrr_rq->size;

	wrr_rq->total_weight -= wrr_entity->weight;

	spin_unlock(&wrr_rq->wrr_rq_lock);
}

/*
 * Called when a task enters a runnable state.
 * It puts the scheduling entity (task) into the red-black tree and
 * increments the nr_running variable.
 */
static void
enqueue_task_wrr(struct rq *rq, struct task_struct *p, int flags)
{
	struct list_head *head;
	struct sched_wrr_entity *new_entity;
	struct sched_wrr_entity *wrr_entity;
	struct wrr_rq *wrr_rq = &rq->wrr;

	wrr_entity = &wrr_rq->run_queue;

	init_task_wrr(p); /* initializes the wrr_entity in task_struct */
	new_entity = &p->wrr;

	/* If on rq already, don't add it */
	if (on_wrr_rq(new_entity))
		return;

	spin_lock(&wrr_rq->wrr_rq_lock);

	/* add it to the queue.*/
	head = &wrr_entity->run_list;
	list_add_tail(&new_entity->run_list, head);

	/* update statistics counts */
	++wrr_rq->nr_running;
	++wrr_rq->size;
	wrr_rq->total_weight += new_entity->weight;

	spin_unlock(&wrr_rq->wrr_rq_lock);
}


/* This function is called when a task voluntarily gives up running */
static void yield_task_wrr(struct rq *rq)
{
	/* So we just requeue the task */
	requeue_task_wrr(rq, rq->curr);
}


static void put_prev_task_wrr(struct rq *rq, struct task_struct *prev)
{

	/* From testing, I have found this method is called
	 * when a task is about to exit.
	 * -> function cgroup_exit() in exit.c # 997
	 * and in the main schedule function.
	 * -> function schedule()  sched.c # 4335
	 */

	/* This method is really just a notification to us before
	 * potentially *another* task that we don't manage/schedule
	 * runs and before a task fully exits.
	 *
	 * As such, we don't need to do anything special.
	 * (in the exiting case, dequeue_task will be called)
	 * */
	update_curr_wrr(rq);
}

#ifdef CONFIG_SMP

#endif

/* This function is mostly called from time tick functions; it might lead to
   process switch.  This drives the running preemption. It is called
   with *interrupts* DISABLED */
static void task_tick_wrr(struct rq *rq, struct task_struct *curr, int queued)
{
	/* Still to be tested  */
	struct timespec now;

	struct sched_wrr_entity *wrr_entity = &curr->wrr;

	getnstimeofday(&now);

	/* Update the current run time statistics. */
	update_curr_wrr(rq);

	if (--wrr_entity->time_left) /* there is still time left */
		return;

	/* the time_slice is in milliseconds and we need to
	 * convert it to ticks units */
	wrr_entity->time_left = wrr_entity->time_slice / SCHED_WRR_TICK_FACTOR;


	/* Requeue to the end of the queue if we are not the only
	 * task on the queue (i.e. if there is more than 1 task) */
	if (wrr_entity->run_list.prev != wrr_entity->run_list.next) {

		requeue_task_wrr(rq, curr);
		/* Set rescheduler for later since this function
		 * is called during a timer interrupt */
		set_tsk_need_resched(curr);
	} else {
		 /* No need for a requeue */
		set_tsk_need_resched(curr);
	}
}

/* This function is called when a currently running task changes its
 * scheduling class or changes its task group. For example, setscheduler
 * calls this function, when a scheduling policy change is requested.
 */
static void set_curr_task_wrr(struct rq *rq)
{
	/* we need to do update our 'curr' pointer.
	 * so that we point to the running task.
	 * rq->curr WILL point to the current running task on the rq.
	 * it's updated in the __schedule() function
	 */
	struct task_struct *p = rq->curr;

	p->se.exec_start = rq->clock_task;

	rq->wrr.curr = &p->wrr;
}

/*
 * This function checks if a task that entered the runnable state should
   preempt the currently running task.
 */
static void check_preempt_curr_wrr(struct rq *rq,
				   struct task_struct *p, int flags)
{
	/* The key for us is that this method is called when a new task
	 * is woken up after sleep. So just enqueue it.
	 *
	 * Well actually, after looking more closely at try_to_wake_up
	 * I found that BOTH enqueue_task AND check_preempt_curr are
	 * CALLED after a task is woken up.
	 *
	 * I am still leaving the enqueue here b'se I have a catch
	 * condition to not add duplicate tasks. */
}

/* This function is called when a running process has changed its scheduler
 * and chosen to make this scheduler (WRR), its scheduler.
 *
 * It is important to note that switched won't be called if there was no
 * actual change in the scheduler class. (e.g. if only the priority number
 * alone changed)
 * */
static void switched_to_wrr(struct rq *rq, struct task_struct *p)
{
	/* From testing, I have found that this method
	 * should not need to do much. We only need to
	 * reset weight to default.
	 *
	 * Note that enqueue will already have been called
	 * by the time we get called.
	 * */
	struct sched_wrr_entity *wrr_entity = &p->wrr;
	wrr_entity->task = p;

	/* Use Default Parameters */
	wrr_entity->weight = SCHED_WRR_DEFAULT_WEIGHT;
	wrr_entity->time_slice =
		SCHED_WRR_DEFAULT_WEIGHT * SCHED_WRR_TIME_QUANTUM;
	wrr_entity->time_left =
		wrr_entity->time_slice / SCHED_WRR_TICK_FACTOR;
}

static void
prio_changed_wrr(struct rq *rq, struct task_struct *p, int oldprio)
{
	/* DO NOT need to implement. Scheduler has no
	 * notion of priority.*/
}

/* Would be called by the get_interval system call
 * sched_rr_ get_interval */
static unsigned int get_rr_interval_wrr(struct rq *rq, struct task_struct *task)
{
	/* To be implemented */
	if (task == NULL)
		return -EINVAL;
	return task->wrr.weight * SCHED_WRR_TIME_QUANTUM /
			SCHED_WRR_TICK_FACTOR ;
}

/* This method is called (through our own mechanims)
 * when a task is forked
 * See sched.c #2903
 * @p = is the newly forked process.
  */
static void task_fork_wrr(struct task_struct *p)
{
	struct sched_wrr_entity *wrr_entity;
	if (p == NULL)
		return;
	wrr_entity = &p->wrr;
	/* initalize the parameters that
	 * should be different for the child process */
	wrr_entity->task = p;

	/* We keep the weight of the parent. We re-initialize
	 * the other values that are derived from the parent's weight */

	wrr_entity->time_slice =
			wrr_entity->weight * SCHED_WRR_TIME_QUANTUM;
	wrr_entity->time_left = wrr_entity->time_slice / SCHED_WRR_TICK_FACTOR;

}

/* Set the SCHED_WRR weight of process, as identified by 'pid'.
 * If 'pid' is 0, set the weight for the calling process.
 * System call number 376.
 */
SYSCALL_DEFINE2(sched_setweight, pid_t, pid, int, weight)
{
	/* We have the following restrictions:
	 * -> Only administrator (root) can change
	 * weight of any process.
	 * -> Only administrator can increase the weight of process
	 * -> A user who user owns the given process may only
	 *    decrease the process weight.
	 * -> It's an error to set weight on process that
	 *    does not have the WRR_POLICY
	 */

	struct task_struct *task = NULL;
	struct pid *pid_struct = NULL;
	struct rq *rq = NULL;
	struct wrr_rq *wrr_rq = NULL;
	int old_weight;

	if (pid < 0)
		return -EINVAL;
	if (weight < 0 || !valid_weight(weight))
		return -EINVAL;

	/* If pid is zero, use the current running task */
	if (pid == 0) {
		task = current;
	} else {
		pid_struct = find_get_pid(pid);
		if (pid_struct == NULL) /* Invalid / Non-existent PID  */
			return -EINVAL;
		task = get_pid_task(pid_struct, PIDTYPE_PID);
	}

	/* Another sanity check */
	if (task == NULL)
		return -EINVAL;

	if (task->policy != SCHED_WRR)
		return -EINVAL;


	/* remember the old_weight */
	old_weight = task->wrr.weight;

	/*
	 * Check if user is root.
	 * Approach borrowed from HW3 solution.
	 */
	if (current_uid() != 0 && current_euid() != 0) { /* Normal user */
		/* normal user can't change other's weights */
		if (!check_same_owner(task))
			return -EPERM;

		/* Normal user can only reduce weight */
		if (weight > task->wrr.weight)
			return -EPERM;

		task->wrr.weight =  weight;
	} else { /* user is root */
		/* anything goes ... */
		task->wrr.weight = (unsigned int) weight;
	}

	/* Update the time slice computation */
	update_timings_after_wt_change(task);

	spin_lock(&SET_WEIGHT_LOCK);

	/* Update the total weight */
	rq = task_rq(task);
	wrr_rq = &rq->wrr;
	wrr_rq->total_weight -= old_weight;
	wrr_rq->total_weight += weight;

	spin_unlock(&SET_WEIGHT_LOCK);
	return 0;
}


/* Obtain the SCHED_WRR weight of a process as identified by 'pid'.
 * If 'pid' is 0, return the weight of the calling process.
 * System call number 377.*/
SYSCALL_DEFINE1(sched_getweight, pid_t, pid)
{
	int result;
	struct task_struct *task = NULL;
	struct pid *pid_struct = NULL;

	if (pid < 0)
		return -EINVAL;

	/* Return weight of current process if PID is 0 */
	if (pid == 0)
		return current->wrr.weight;

	pid_struct = find_get_pid(pid);

	if (pid_struct == NULL)/* Invalid / Non-existent PID  */
		return -EINVAL;


	task = get_pid_task(pid_struct, PIDTYPE_PID);

	result = task->wrr.weight;

	return result;
}

/* ========  Multiple CPUs Scheduling Functions Below =========*/
#ifdef CONFIG_SMP


/* performs wrr rq loading balance. */
static void wrr_rq_load_balance(void)
{
	int cpu;
	int dest_cpu; /* id of cpu to move to */
	struct rq *rq;
	struct wrr_rq *lowest_wrr_rq, *highest_wrr_rq, *curr_wrr_rq;
	struct sched_wrr_entity *heaviest_task_on_highest_wrr_rq;
	struct sched_wrr_entity *curr_entity;
	struct list_head *curr;
	struct list_head *head;
	struct task_struct *task_to_move;
	struct rq *rq_of_task_to_move;
	struct rq *rq_of_lowest_wrr; /*rq of thing with smallest weight */

	int counter = 1;
	int lowest_weight = INT_MAX;
	int highest_weight = INT_MIN;

	int largest_weight = INT_MIN; /* used for load imblance issues*/

	curr_entity = NULL;

	for_each_online_cpu(cpu) {
		rq = cpu_rq(cpu);
		if (rq == NULL)
			continue;

		curr_wrr_rq = &rq->wrr;
		if (curr_wrr_rq->total_weight > highest_weight) {
			highest_wrr_rq = curr_wrr_rq;
			highest_weight = curr_wrr_rq->total_weight;
		}
		if (curr_wrr_rq->total_weight < lowest_weight) {
			lowest_wrr_rq = curr_wrr_rq;
			lowest_weight = curr_wrr_rq->total_weight;
		}
		/* confirm if you don't have to do anything extra */
		++counter;
	}

	if (lowest_wrr_rq == highest_wrr_rq)
		return;
	/* See if we can do move  */
	/* Need to make sure that we don't cause a load imbalance */
	head = &highest_wrr_rq->run_queue.run_list;
	spin_lock(&highest_wrr_rq->wrr_rq_lock);
	for (curr = head->next; curr != head; curr = curr->next) {
		curr_entity = list_entry(curr,
					struct sched_wrr_entity,
					run_list);

		if (curr_entity->weight > largest_weight) {
			heaviest_task_on_highest_wrr_rq = curr_entity;
			largest_weight = curr_entity->weight;
		}
	}
	spin_unlock(&highest_wrr_rq->wrr_rq_lock);

	if (heaviest_task_on_highest_wrr_rq->weight +
			lowest_wrr_rq->total_weight >=
				highest_wrr_rq->total_weight)
		/* there is an imbalance issues here */ {
		return;
	}
	/* Okay, let's move the task */
	rq_of_lowest_wrr = container_of(lowest_wrr_rq, struct rq, wrr);
	dest_cpu = rq_of_lowest_wrr->cpu;
	task_to_move = container_of(heaviest_task_on_highest_wrr_rq,
				    struct task_struct, wrr);

	rq_of_task_to_move = task_rq(task_to_move);
	deactivate_task(rq_of_task_to_move, task_to_move, 0);

	set_task_cpu(task_to_move, dest_cpu);
	activate_task(rq_of_lowest_wrr , task_to_move, 0);
}

/* Find the CPU with the lightest load
 * @do_lock indicates if we should lock. */
static int find_lightest_cpu_runqueue(void)
{
	struct rq *rq;
	struct wrr_rq *wrr_rq;
	int cpu, best_cpu, counter, weight;
	int lowest_weight = INT_MAX;
	counter = 0;

	best_cpu = -1; /* assume no best cpu */
	for_each_online_cpu(cpu) {
		rq = cpu_rq(cpu);
		wrr_rq = &rq->wrr;
		weight = wrr_rq->total_weight;

		if (weight < lowest_weight) {
			lowest_weight = weight;
			best_cpu = cpu;
		}

		++counter;
	}

	return best_cpu;
}


/* Assumes rq->lock is held */
static void rq_online_wrr(struct rq *rq)
{
	/* Called when a CPU goes online  */
	/*Don't think necessary for our puporses */

}

/* Assumes rq->lock is held */
static void rq_offline_wrr(struct rq *rq)
{
	/* Called when a CPU goes offline  */
	/*Don't think necessary for our puporses */
}


/*
 * When switch from the rt queue, we bring ourselves to a position
 * that we might want to pull RT tasks from other runqueues.
 */
static void switched_from_wrr(struct rq *rq, struct task_struct *p)
{
	/* This is just a heads up that the current task is going
	 * to be switched *very* soon */
}


static void pre_schedule_wrr(struct rq *rq, struct task_struct *prev)
{
	/* From looking at sched.c don't think this is needed */
}


static void post_schedule_wrr(struct rq *rq)
{
	/* From looking at sched.c don't think this is needed */
}


/*
 * If we are not running and we are not going to reschedule soon, we should
 * try to push tasks away now
 */
static void task_woken_wrr(struct rq *rq, struct task_struct *p)
{
	/* Called when a task is woken up */
}

static int
select_task_rq_wrr(struct task_struct *p, int sd_flag, int flags)
{
	/* find lightest returns -1 on error */
	int lowest_cpu = find_lightest_cpu_runqueue();

	if (lowest_cpu == -1)
		return task_cpu(p);
	else
		return lowest_cpu;
}

#endif /* CONFIG_SMP */

/*
 * Simple, special scheduling class for the per-CPU wrr tasks:
 */
static const struct sched_class wrr_sched_class = {
	/* .next is Fair Scheduler class scheduler */
	.next			= &fair_sched_class,

	.enqueue_task		= enqueue_task_wrr,

	.yield_task		= yield_task_wrr,

	.dequeue_task		= dequeue_task_wrr,

	.check_preempt_curr	= check_preempt_curr_wrr,

	.pick_next_task		= pick_next_task_wrr,
	.put_prev_task		= put_prev_task_wrr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_wrr,
	.rq_online              = rq_online_wrr,
	.rq_offline             = rq_offline_wrr,
	.pre_schedule		= pre_schedule_wrr,
	.post_schedule		= post_schedule_wrr,
	.task_woken		= task_woken_wrr,
	.switched_from		= switched_from_wrr,
#endif

	.set_curr_task          = set_curr_task_wrr,
	.task_tick		= task_tick_wrr,

	.get_rr_interval	= get_rr_interval_wrr,

	.prio_changed		= prio_changed_wrr,
	.switched_to		= switched_to_wrr,
};
