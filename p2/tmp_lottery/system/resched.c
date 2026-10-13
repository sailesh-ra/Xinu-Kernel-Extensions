/* resched.c - resched, resched_cntl */

#include <xinu.h>
#include <stdlib.h> 

//#define DEBUG_CTXSW

struct	defer	Defer;

/*------------------------------------------------------------------------
 *  resched  -  Reschedule processor to highest priority eligible process
 *------------------------------------------------------------------------
 */
void	resched(void)		/* Assumes interrupts are disabled	*/
{
	struct procent *ptold;	/* Ptr to table entry for old process	*/
	struct procent *ptnew;	/* Ptr to table entry for new process	*/
	pid32 old_pid;

	/* Lottery parameters */
	qid16 curr;				/* Iterator for queue */
	uint32 total_tickets = 0;	/* Total number of tickets for scheduling */
	uint32 user_process_count = 0;	/* Number of user processes */
	uint32 winner = 0;			/* Winner process */
	uint32 search_counter = 0;
	//srand(1);

	/* If rescheduling is deferred, record attempt and return */

	if (Defer.ndefers > 0) {
		Defer.attempt = TRUE;
		return;
	}

	/* Point to process table entry for the current (old) process */

	ptold = &proctab[currpid];
	old_pid = currpid;

	if (ptold->prstate == PR_CURR) {

		/* Old process will no longer remain current */
		ptold->prstate = PR_READY;

		if (ptold->user_process == FALSE) {
			insert(currpid, readylist, ptold->prprio);
		}
		else {
			insert_to_user(currpid, userprocesslist, ptold->tickets);
		}
	}

	/* Scheduling decision starts here 																	 */
	/* Based on the context, there are a total 3 cases possible 										 */
	/* Case 1: There are processes in readylist(other than nullproc), then schedule these system process */
	/* Case 2: There are no system processes except nullproc, schedule user process from userprocesslist */
	/*		SubCase: There is only 1 process and with zero tickets, skip it and schedule nullproc		 */
	/*		SubCase: There is only 1 user process with tickets, schedule it directly					 */
	/* Case 3: No processes are available, schedule nullproc											 */

	/* Case 1 */
	if (firstkey(readylist) != (pid32)(0)) {
		currpid = dequeue(readylist);
	}

	/* Case 2 */
	else if (!isempty(userprocesslist)) {
		bool8 no_lottery = FALSE;
		
		// Check if the only user process has 0 tickets
		curr = firstid(userprocesslist);

		if (queuetab[curr].qkey == 0) {
			currpid = dequeue(readylist);
			no_lottery = TRUE;
		}
		
		if (no_lottery == FALSE) {
			/* Calculate tickets */
			curr = firstid(userprocesslist);
			while (curr != queuetail(userprocesslist)) {
				user_process_count++;
				total_tickets += queuetab[curr].qkey;
				curr = queuetab[curr].qnext;
			}
			// Do lottery if more than 1 user process present
			if (user_process_count > 1) {
				/* Find the winner */
				winner = rand() % total_tickets;
				/* Loop through to find the winner process */
				curr = firstid(userprocesslist);
				while (curr != queuetail(userprocesslist)) {
					search_counter += queuetab[curr].qkey;
					if(search_counter > winner)
						break;
					curr = queuetab[curr].qnext;
				}
				// Assign winner process
				currpid = curr;
				
				/* Remove winner from userprocesslist*/
				queuetab[queuetab[currpid].qprev].qnext = queuetab[currpid].qnext;
				queuetab[queuetab[currpid].qnext].qprev = queuetab[currpid].qprev;
				queuetab[currpid].qnext = EMPTY;
				queuetab[currpid].qprev = EMPTY;
			}
			// Directly schedule if only 1 process
			else {
				currpid = dequeue(userprocesslist);
			}
		}
	}

	/* Case 3 */
	else {
		currpid = dequeue(readylist);
	}
	
	ptnew = &proctab[currpid];
	ptnew->prstate = PR_CURR;
	preempt = QUANTUM;		/* Reset time slice for process	*/

	if(old_pid != currpid) {
		ptnew->num_ctxsw++;
	}
	
	/* DEBUG_CTXSW directive */
	#ifdef DEBUG_CTXSW
	if(old_pid != new_pid)
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