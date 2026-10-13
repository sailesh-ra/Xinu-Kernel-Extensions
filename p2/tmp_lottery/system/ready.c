/* ready.c - ready */

#include <xinu.h>

qid16	readylist;			/* Index of ready list		*/
qid16	userprocesslist;

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
	if (prptr->user_process == FALSE) {
		insert(pid, readylist, prptr->prprio);
	}
	else {
		insert_to_user(pid, userprocesslist, prptr->tickets);
	}
	resched();

	return OK;
}

/*------------------------------------------------------------------------
 * print_ready_list - print the identifiers of processes in ready list
 *------------------------------------------------------------------------
 */

syscall print_ready_list(qid16 q)
{
    qid16   curr;

    if(isbadqid(q))
		return SYSERR;
	if (isempty(q)) {
		kprintf("Queue is empty...");
		return OK;
	}
    
    kprintf("Ready list: ");
    
    curr = firstid(q);
    
    while (curr != queuetail(q))
	{
		kprintf("Element %d: , Key: %d \n", curr, queuetab[curr].qkey);
		curr = queuetab[curr].qnext;
	}

	return OK;
}
