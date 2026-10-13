/* vcreate.c - vcreate */

#include <xinu.h>
#include <stdarg.h>

//#define DEBUG_VCREATE
//#define DEBUG

local	pid32	newpid(void);

/*------------------------------------------------------------------------
 * vcreate - Create a process with virtual memory support
 *------------------------------------------------------------------------
 */
pid32 vcreate(
	void	*funcaddr,	/* Address of function		*/
	uint32	ssize,		/* Stack size in bytes		*/
	pri16	priority,	/* Process priority > 0		*/
	char	*name,		/* Name (for debugging)		*/
	uint32	nargs,		/* Number of arguments		*/
	...			/* Arguments for the process	*/
)
{
	uint32		savsp, *pushsp;
	intmask		mask;
	pid32		pid;
	struct procent	*prptr;
	int32		i;
	uint32		*saddr;
	pd_t		*pd;
	struct vmemblk	*vmemptr;
    uint32 *a;

	mask = disable();

	#ifdef DEBUG_VCREATE
	kprintf("[vcreate] Creating process '%s', prio=%d, ssize=%d\n", name, priority, ssize);
	#endif

	/* Validate and round stack size */
	if (ssize < MINSTK) {
		ssize = MINSTK;
	}
	ssize = (uint32)roundmb(ssize);

	/* Allocate PID and stack */
	if ((priority < 1) ||
	    ((pid = newpid()) == SYSERR) ||
	    ((saddr = (uint32 *)getstk(ssize)) == (uint32 *)SYSERR)) {
		restore(mask);
		return SYSERR;
	}

	prcount++;
	prptr = &proctab[pid];

	/* Initialize basic process table entry (same as create()) */
	prptr->prstate = PR_SUSP;
	prptr->prprio = priority;
	prptr->prstkbase = (char *)saddr;
	prptr->prstklen = ssize;
	prptr->prname[PNMLEN-1] = NULLCH;
	for (i = 0; i < PNMLEN-1 && (prptr->prname[i] = name[i]) != NULLCH; i++)
		;
	prptr->prsem = -1;
	prptr->prparent = (pid32)getpid();
	prptr->prhasmsg = FALSE;

	/* Set up stdin, stdout, stderr */
	prptr->prdesc[0] = CONSOLE;
	prptr->prdesc[1] = CONSOLE;
	prptr->prdesc[2] = CONSOLE;

	/* ===== MARK AS USER PROCESS ===== */
	prptr->pruser = TRUE;

	/* ===== VIRTUAL MEMORY SETUP ===== */

	/* Allocate page directory for this process */
	pd = (pd_t *)getpage();
	if (pd == (pd_t *)SYSERR) {
		freestk((char *)saddr, ssize);
		proctab[pid].prstate = PR_FREE;
		prcount--;
		restore(mask);
		return SYSERR;
	}
	memset(pd, 0, PAGE_SIZE);
	prptr->prpd = pd;

	/* Identity-map entire XINU area (TEXT/DATA/BSS/shared heap/stacks) */
	if (map_identity_range(pd, 0x00000000, 0x02000000) == SYSERR) {
		freepage((uint32 *)pd);
		freestk((char *)saddr, ssize);
		proctab[pid].prstate = PR_FREE;
		prcount--;
		restore(mask);
		return SYSERR;
	}

	/* ===== INITIALIZE PRIVATE VIRTUAL HEAP ===== */

	/* Set heap boundaries (lazy allocation - no physical frames yet) */
	prptr->prheapbase = 0x02000000;     /* Right after XINU area (32 MB) */
	prptr->prheaplimit = 0x42000000;    /* 1 GB virtual heap */

	/* Create initial free list: entire heap is FREE */
	vmemptr = (struct vmemblk *)getmem(sizeof(struct vmemblk));
	if (vmemptr == (struct vmemblk *)SYSERR) {
		freepage((uint32 *)pd);
	#ifdef DEBUG_VCREATE
    kprintf("[cleanup_paging] After freepage, about to free vmemblk\n");
	#endif        
		freestk((char *)saddr, ssize);
		proctab[pid].prstate = PR_FREE;
		prcount--;
		restore(mask);
		return SYSERR;
	}

	vmemptr->vnext = NULL;
	vmemptr->vaddr = prptr->prheapbase;
	vmemptr->vlength = prptr->prheaplimit - prptr->prheapbase;
	prptr->prvmlist = vmemptr;

	#ifdef DEBUG_VCREATE
	kprintf("[vcreate] P%d created: heap=0x%08x-0x%08x, PD=0x%08x\n",
			pid, prptr->prheapbase, prptr->prheaplimit, pd);
	#endif

	/* ===== STACK INITIALIZATION (same as create()) ===== */

	*saddr = STACKMAGIC;
	savsp = (uint32)saddr;

    /* Push arguments */
    a = (uint32 *)(&nargs + 1);     /* Start of args        */
    a += nargs - 1;                  /* Last argument        */
    for ( ; nargs > 0 ; nargs--)    /* Machine dependent; copy args */
        *--saddr = *a--;            /* onto created process's stack */

	*--saddr = (long)INITRET;

	/* Set up for ctxsw */
	*--saddr = (long)funcaddr;
	*--saddr = savsp;
	savsp = (uint32)saddr;
	*--saddr = 0x00000200;	/* eflags with interrupts enabled */

	/* Emulate pushal */
	*--saddr = 0;		/* eax */
	*--saddr = 0;		/* ecx */
	*--saddr = 0;		/* edx */
	*--saddr = 0;		/* ebx */
	*--saddr = 0;		/* esp (placeholder) */
	pushsp = saddr;
	*--saddr = savsp;	/* ebp */
	*--saddr = 0;		/* esi */
	*--saddr = 0;		/* edi */
	*pushsp = (unsigned long)(prptr->prstkptr = (char *)saddr);

	#ifdef DEBUG_VCREATE
	kprintf("[vcreate] P%d ready to run\n", pid);
	#endif

    #ifdef DEBUG
    // Debug: verify stack layout
    uint32 *check = (uint32 *)prptr->prstkptr;
    kprintf("Stack verification:\n");
    kprintf("  prstkptr = 0x%08x\n", (uint32)check);
    kprintf("  [+9]  funcaddr slot: 0x%08x (expect 0x%08x)\n", check[9], (uint32)funcaddr);
    kprintf("  [+10] INITRET slot:  0x%08x (expect 0x%08x)\n", check[10], (uint32)INITRET);
    kprintf("  [+11] STACKMAGIC:    0x%08x (expect 0x%08x)\n", check[11], STACKMAGIC);
    #endif

	restore(mask);
	return pid;
}

/*------------------------------------------------------------------------
 * newpid  -  Obtain a new (free) process ID
 *------------------------------------------------------------------------
 */
local pid32 newpid(void)
{
	uint32	i;
	static	pid32 nextpid = 1;

	for (i = 0; i < NPROC; i++) {
		nextpid %= NPROC;
		if (proctab[nextpid].prstate == PR_FREE) {
			return nextpid++;
		} else {
			nextpid++;
		}
	}
	return (pid32)SYSERR;
}