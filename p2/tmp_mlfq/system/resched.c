/* resched.c - resched, resched_cntl */

#include <xinu.h>

//#define DEBUG_CTXSW

struct	defer	Defer;
bool8 boost_flag = FALSE;

/*------------------------------------------------------------------------
 *  resched  -  Reschedule processor to highest priority eligible process
 *------------------------------------------------------------------------
 */
void	resched(void)		/* Assumes interrupts are disabled	*/
{
	struct procent *ptold;	/* Ptr to table entry for old process	*/
	struct procent *ptnew;	/* Ptr to table entry for new process	*/
	pid32 old_pid;

	/* If rescheduling is deferred, record attempt and return */

	if (Defer.ndefers > 0) {
		Defer.attempt = TRUE;
		return;
	}

	/* Point to process table entry for the current (old) process */

	ptold = &proctab[currpid];
	old_pid = currpid;

	if ((ptold->prstate == PR_CURR)) {  /* Process remains eligible */
		
		/*if ((ptold->prprio > firstkey(readylist)) && (ptold->user_process == FALSE)) {
			return;
		}*/

		/* Old process will no longer remain current */
		ptold->prstate = PR_READY;

		/* System Process */
		if (ptold->user_process == FALSE) {
			insert(currpid, readylist, ptold->prprio);
		}

		/* User Process */
		else {
			/* Check for number of queues */

			/* If 3 queues */
			#ifdef UPRIORITY_QUEUES
				/* If in high */
				if(ptold->prprio == 10) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT) {
						enqueue(currpid, mlfq_high);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 20;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_med);
					}
					//print_ready_list(mlfq_high);
				}
				/* If in medium */	
				else if(ptold->prprio == 20) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT*2) {
						enqueue(currpid, mlfq_med);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 30;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_low);
					}
					//print_ready_list(mlfq_med);
				}
				/* If in low */
				else {
					ptold->prprio = 30;
					enqueue(currpid, mlfq_low);
					//print_ready_list(mlfq_low);
				}
			#endif

			/* If 5 queues */
			#ifndef UPRIORITY_QUEUES
				/* If in highest */
				if(ptold->prprio == 10) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT) {
						enqueue(currpid, mlfq_highest);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 20;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_high);
					}
				}
				/* If in high */	
				else if(ptold->prprio == 20) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT*2) {
						enqueue(currpid, mlfq_high);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 30;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_med);
					}
				}
				/* If in medium */	
				else if(ptold->prprio == 30) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT*2) {
						enqueue(currpid, mlfq_med);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 40;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_low);
					}
				}
				/* If in low */	
				else if(ptold->prprio == 40) {
					/* If it still has time to run*/
					if(ptold->queue_time < TIME_ALLOTMENT*2) {
						enqueue(currpid, mlfq_low);
					}
					/* If it's time is exhausted*/
					else {
						ptold->downgrades++;
						ptold->prprio = 50;
						ptold->queue_time = 0;
						enqueue(currpid, mlfq_lowest);
					}
				}
				/* If in lowest */
				else {
					ptold->prprio = 50;
					enqueue(currpid, mlfq_lowest);
				}
			#endif
			}		
		}

	/* Scheduling decision starts here 																	 */
	/* Based on the context, there are a total 3 cases possible 										 */
	/* Case 1: There are processes in readylist(other than nullproc), then schedule these system process */
	/*		SubCase: Search process from high priority queue and move down till found					 */
	/*		SubCase: If multiple process in a queue -> RR		 								 		 */
	/* Case 2: There are no system processes except nullproc, schedule user process from userprocesslist */
	/* Case 3: No processes are available, schedule nullproc											 */

	//kprintf("\nboost_flag: %d", boost_flag);
	
	if (boost_flag == TRUE) {
		boost_priority();
	}
	
	//kprintf("\ncurrpid: %d", currpid);
	/* Case 1: Schedule System Process */
	if (firstkey(readylist) != (pid32)(0)) {
		currpid = dequeue(readylist);
		preempt = QUANTUM;
	}
	
	/* Case 2: Schedule User Process */

	/* If 3 queues */
	#ifdef UPRIORITY_QUEUES
		else if (nonempty(mlfq_high)) {
			currpid = dequeue(mlfq_high);
			preempt = QUANTUM;
		}

		else if (nonempty(mlfq_med)) {
			currpid = dequeue(mlfq_med);
			preempt = QUANTUM*2;
		}

		else if (nonempty(mlfq_low)) {
			currpid = dequeue(mlfq_low);
			preempt = QUANTUM*4;
		}

	#endif
	
	/* If 5 queues */
	#ifndef UPRIORITY_QUEUES
		else if (nonempty(mlfq_highest)) {
			currpid = dequeue(mlfq_highest);
			preempt = QUANTUM;
		}

		else if (nonempty(mlfq_high)) {
			currpid = dequeue(mlfq_high);
			preempt = QUANTUM*2;
		}

		else if (nonempty(mlfq_med)) {
			currpid = dequeue(mlfq_med);
			preempt = QUANTUM*4;
		}

		else if (nonempty(mlfq_low)) {
			currpid = dequeue(mlfq_low);
			preempt = QUANTUM*8;
		}

		else if (nonempty(mlfq_lowest)) {
			currpid = dequeue(mlfq_lowest);
			preempt = QUANTUM*16;
		}

	#endif

	/* Case 3: Schedule Null Process */
	else {
		currpid = dequeue(readylist);
		preempt = QUANTUM;
	}
	
	//kprintf("\nScheduled PID: %d", currpid);
	/* Context Switch */
	ptnew = &proctab[currpid];
	ptnew->prstate = PR_CURR;

	if(old_pid != currpid) {
		ptnew->num_ctxsw++;
	}
	
	/* DEBUG_CTXSW directive */
	#ifdef DEBUG_CTXSW
	if(old_pid != currpid)
		kprintf("ctxsw::%d-%d\n", old_pid, currpid);
	#endif

	ctxsw(&ptold->prstkptr, &ptnew->prstkptr);

	/* Old process returns here when resumed */

	return;
}

/*------------------------------------------------------------------------
 *  resched_cntl  -  Control whether rescheduling is deferred or allowed
 *------------------------------------------------------------------------
 */
status	resched_cntl(		/* Assumes interrupts are disabled	*/
	  int32	defer		/* Either DEFER_START or DEFER_STOP	*/
	)
{
	switch (defer) {

	    case DEFER_START:	/* Handle a deferral request */

		if (Defer.ndefers++ == 0) {
			Defer.attempt = FALSE;
		}
		return OK;

	    case DEFER_STOP:	/* Handle end of deferral */
		if (Defer.ndefers <= 0) {
			return SYSERR;
		}
		if ( (--Defer.ndefers == 0) && Defer.attempt ) {
			resched();
		}
		return OK;

	    default:
		return SYSERR;
	}
}

/* Boost the proiority of all user processes */
void boost_priority()
{
	pid32 pid;

	#ifdef UPRIORITY_QUEUES
		/* Medium priority queue */
		while ((!isbadqid(mlfq_med)) && nonempty(mlfq_med)) {
		    pid = dequeue(mlfq_med);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_high);  // Move to the high-priority queue
		}

		/* Low priority queue */

		while ((!isbadqid(mlfq_low)) && nonempty(mlfq_low)) {
			pid = dequeue(mlfq_low);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_high);  // Move to the high-priority queue
		}
	
	#endif

	#ifndef UPRIORITY_QUEUES
		/* High priority queue */
		while ((!isbadqid(mlfq_high)) && nonempty(mlfq_high)) {
		    pid = dequeue(mlfq_high);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_highest);  // Move to the high-priority queue
		}

		/* Medium priority queue */
		while ((!isbadqid(mlfq_med)) && nonempty(mlfq_med)) {
		    pid = dequeue(mlfq_med);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_highest);  // Move to the high-priority queue
		}

		/* Low priority queue */

		while ((!isbadqid(mlfq_low)) && nonempty(mlfq_low)) {
			pid = dequeue(mlfq_low);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_highest);  // Move to the high-priority queue
		}

		/* Low priority queue */

		while ((!isbadqid(mlfq_lowest)) && nonempty(mlfq_lowest)) {
			pid = dequeue(mlfq_lowest);  // Remove from the medium queue
		    proctab[pid].prprio = 10;
		    proctab[pid].queue_time = 0;
		    proctab[pid].upgrades++;
		    enqueue(pid, mlfq_highest);  // Move to the high-priority queue
		}

	#endif
	
	boost_flag = FALSE;

	//print_ready_list(mlfq_high);
	//print_ready_list(mlfq_med);
	//print_ready_list(mlfq_low);

	//kprintf("BOOST DONE");
}

/* reset counter used to trigger priority upgrades */
void 	reset_timing()
{
	ctr1000 = 0;
}