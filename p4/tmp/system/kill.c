/* kill.c - kill */

#include <xinu.h>

/* Debug control */
//#define KILL_DEBUG
//#define KILL_PAGING_DEBUG

/* Forward declaration */
void cleanup_paging(struct procent *prptr, pid32 pid);

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
	int32	i;			/* Index into descriptors	*/

	mask = disable();

	#ifdef KILL_DEBUG
	kprintf("[kill] P%d: killing process, state=%d\n", pid, proctab[pid].prstate);
	#endif
	if (isbadpid(pid) || (pid == NULLPROC)
	    || ((prptr = &proctab[pid])->prstate) == PR_FREE) {
		restore(mask);
		return SYSERR;
	}

	if (--prcount <= 1) {		/* Last user process completes	*/
		xdone();
	}

	send(prptr->prparent, pid);
	for (i=0; i<3; i++) {
		close(prptr->prdesc[i]);
	}
	freestk(prptr->prstkbase, prptr->prstklen);

	/*
	 * ========== PAGING CLEANUP FOR USER PROCESSES ==========
	 * For user processes (created with vcreate), we must free:
	 * 1. All FFS frames in use (pages with pt_pres == 1)
	 * 2. All page tables allocated for the process
	 * 3. The page directory itself
	 */
	if (prptr->pruser == TRUE && prptr->prpd != NULL) {
		#ifdef KILL_DEBUG
		kprintf("[kill] P%d: User process, cleaning up paging\n", pid);
		#endif
		#ifdef KILL_PAGING_DEBUG
		kprintf("[kill] P%d: Cleaning up paging structures\n", pid);
		#endif

		cleanup_paging(prptr, pid);

		#ifdef KILL_DEBUG
		kprintf("[kill] P%d: Paging cleanup complete\n", pid);
		#endif
	}

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

	#ifdef KILL_DEBUG
	kprintf("[kill] P%d: Process killed successfully\n", pid);
	#endif

	restore(mask);
	return OK;
}

/*
 *  cleanup_paging  -  Free all paging resources for a user process
 */



void cleanup_paging(struct procent *prptr,pid32 pid) {
	pd_t *pd;
	pt_t *pt;
	uint32 pd_index, pt_index;
	uint32 vaddr;
	uint32 saved_cr3;
	uint32 pt_freed = 0;

	/* Switch to kernel CR3 FIRST to access FFS table and page tables */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);

	/* Free all FFS frames and swap frames for this process */
	#ifdef KILL_PAGING_DEBUG
	kprintf("[cleanup_paging] ENTERED\n");
	kprintf("[cleanup_paging] About to call free_process_ffs\n");
	#endif
	free_process_ffs(pid);

	#ifdef KILL_PAGING_DEBUG
	kprintf("[cleanup_paging] About to call free_process_swap\n");
	#endif
	free_process_swap(pid);

	#ifdef KILL_PAGING_DEBUG
	kprintf("[cleanup_paging] Starting page table cleanup\n");
	kprintf("[cleanup_paging] PD=0x%08x, heap=0x%08x-0x%08x\n",
			prptr->prpd, prptr->prheapbase, prptr->prheaplimit);
	#endif
	
	pd = prptr->prpd;
	
	/* Don't free the kernel's page directory! */
	if (pd == pd_base_addr) {
		#ifdef KILL_PAGING_DEBUG
		kprintf("[cleanup_paging] Skipping kernel PD\n");
		#endif
		write_cr3(saved_cr3);
		return;
	}
	
	/*
	 * Walk the page directory and page tables to free page tables
	 * NOTE: FFS frames already freed by free_process_ffs() above
	 */
	for (pd_index = 0; pd_index < 1024; pd_index++) {
		if (pd[pd_index].pd_pres == 0) {
			continue;  /* No page table here */
		}

		pt = (pt_t *)(pd[pd_index].pd_base << 12);

		#ifdef KILL_PAGING_DEBUG
		kprintf("[cleanup_paging] PD[%d] -> PT at 0x%08x\n", pd_index, pt);
		#endif
		
		uint32 pd_vaddr_start = pd_index << 22;
		if (pd_vaddr_start >= prptr->prheapbase) {
			#ifdef KILL_PAGING_DEBUG
			kprintf("[cleanup_paging]   Freeing PT at 0x%08x\n", pt);
			#endif
			freepage((uint32 *)pt);
			
			#ifdef KILL_PAGING_DEBUG
			kprintf("[cleanup_paging] PD freed OK\n");
			#endif
			pt_freed++;
		}
	}
	
	/* Free the page directory itself */
	#ifdef KILL_PAGING_DEBUG
	kprintf("[cleanup_paging] Freeing PD at 0x%08x\n", pd);
	#endif
	freepage((uint32 *)pd);
	
	/* Free any remaining vmemblk nodes in prvmlist */
	struct vmemblk *curr, *next;
	curr = prptr->prvmlist;
	while (curr != NULL) {
		next = curr->vnext;
		freemem((char *)curr, sizeof(struct vmemblk));
		curr = next;
	}
	prptr->prvmlist = NULL;
	
	/* Clear paging fields */
	prptr->prpd = NULL;
	prptr->prheapbase = 0;
	prptr->prheaplimit = 0;
	prptr->pruser = FALSE;


	#ifdef KILL_PAGING_DEBUG
	kprintf("[cleanup_paging] Done: freed %d page tables\n", pt_freed);
	#endif
}


