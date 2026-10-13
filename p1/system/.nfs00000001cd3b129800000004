/* kill.c - kill */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  kill  -  Kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
syscall	kill(
	  pid32		pid		/* ID of process to kill	*/
	)
{
	intmask	mask;			/* Saved interrupt mask		*/
	struct	procent *prptr;		/* Ptr to process's table entry	*/
	struct 	procent	*childptr;	/* Ptr to find potential child processes*/
	int32	i;			/* Index into descriptors	*/

	mask = disable();
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}
	
	/* Sends it's pid to the parent process */
	send(prptr->prparent, pid);

	/* Find child process(es) and kill them */
	if(prptr->user_process) {
		for (i=1; i<NPROC; i++) {
			if((childptr = &proctab[i])->prparent == pid) {
				kill(i);
			}
		}
	}

	/* Closes all the file descriptors of this process*/
	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}

	/* Free the process's stack*/
	freestk(prptr->prstkbase, prptr->prstklen);

	/* Change the process state */
	switch (prptr->prstate) {
	case PR_CURR:
		prptr->prstate = PR_FREE;	/* Suicide */
		resched();

	case PR_SLEEP:
	case PR_RECTIM:
		unsleep(pid);
		prptr->prstate = PR_FREE;
		break;

	case PR_WAIT:
		semtab[prptr->prsem].scount++;
		/* Fall through */

	case PR_READY:
		getitem(pid);		/* Remove from queue */
		/* Fall through */

	default:
		prptr->prstate = PR_FREE;
	}

	restore(mask);
	return OK;
}


