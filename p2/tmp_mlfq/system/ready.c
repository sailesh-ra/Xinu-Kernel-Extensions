/* ready.c - ready */

#include <xinu.h>

qid16	readylist;			/* Index of ready list		*/

#ifdef UPRIORITY_QUEUES 
	qid16	mlfq_high;		/* MLFQ queue 0*/
	qid16	mlfq_med;		/* MLFQ queue 1*/
	qid16	mlfq_low;		/* MLFQ queue 2*/
#endif

#ifndef UPRIORITY_QUEUES
	qid16	mlfq_highest;	/* MLFQ queue 0*/
	qid16	mlfq_high;		/* MLFQ queue 1*/
	qid16	mlfq_med;		/* MLFQ queue 2*/
	qid16	mlfq_low;		/* MLFQ queue 2*/
	qid16	mlfq_lowest;	/* MLFQ queue 2*/
#endif

/*------------------------------------------------------------------------
 *  ready  -  Make a process eligible for CPU service
 *------------------------------------------------------------------------
 */
status	ready(
	  pid32		pid		/* ID of process to make ready	*/
	)
{
	register struct procent *prptr;

	if (isbadpid(pid)) {
		return SYSERR;
	}

	/* Set process state to indicate ready and add to ready list */

	prptr = &proctab[pid];
	prptr->prstate = PR_READY;

	if (prptr->user_process == FALSE) {
		insert(pid, readylist, prptr->prprio);
	}
	else {
		/* Check for number of queues */

		/* If 3 queues */
		#ifdef UPRIORITY_QUEUES
			/* If in high */
			if(prptr->prprio == 10) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT) {
					enqueue(pid, mlfq_high);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 20;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_med);
				}
				//print_ready_list(mlfq_high);
			}
			/* If in medium */	
			else if(prptr->prprio == 20) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT*2) {
					enqueue(pid, mlfq_med);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 30;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_low);
				}
				//print_ready_list(mlfq_med);
			}
			/* If in low */
			else {
				prptr->prprio = 30;
				enqueue(pid, mlfq_low);
				//print_ready_list(mlfq_low);
			}

		#endif

		/* If 5 queues */
		#ifndef UPRIORITY_QUEUES
			/* If in highest */
			if(prptr->prprio == 10) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT) {
					enqueue(pid, mlfq_highest);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 20;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_high);
				}
			}
			/* If in high */	
			else if(prptr->prprio == 20) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT*2) {
					enqueue(pid, mlfq_high);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 30;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_med);
				}
			}
			/* If in medium */	
			else if(prptr->prprio == 30) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT*2) {
					enqueue(pid, mlfq_med);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 40;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_low);
				}
			}
			/* If in low */	
			else if(prptr->prprio == 40) {
				/* If it still has time to run*/
				if(prptr->queue_time < TIME_ALLOTMENT*2) {
					enqueue(pid, mlfq_low);
				}
				/* If it's time is exhausted*/
				else {
					prptr->downgrades++;
					prptr->prprio = 50;
					prptr->queue_time = 0;
					enqueue(pid, mlfq_lowest);
				}
			}
			/* If in lowest */
			else {
				prptr->prprio = 50;
				enqueue(pid, mlfq_lowest);
			}

		#endif
	}
	
	resched();

	return OK;
}

/*---------------------------------------------------------------------------------------------------------------
 *  print_ready_list  -  Prints queue contents but unlike the name suggests, 
 *			 			 it can be used to print any queue
 *--------------------------------------------------------------------------------------------------------------
 */
syscall print_ready_list(qid16 q)
{
	qid16 curr;			/* index iterator */
	
	if(isbadqid(q))
		return SYSERR;
	if (isempty(q)) {
		kprintf("Queue is empty...");
		return OK;
	}

	curr = firstid(q);
	
	while (curr != queuetail(q))
	{
		kprintf("Element: %d , Key: %d \n", curr, queuetab[curr].qkey);
		curr = queuetab[curr].qnext;
	}

	return OK;
}