/* queue.h - firstid, firstkey, isempty, lastkey, nonempty		*/

/* Queue structure declarations, constants, and inline functions	*/

/* Default # of queue entries: 1 per process plus 2 for ready list plus	*/
/* 2 for sleep list plus 2 per semaphore plus 2 for user process queue	*/
#ifndef NQENT
#define NQENT	(NPROC + 4 + NSEM + NSEM + 2)
#endif

#define	EMPTY	(-1)		/* Null value for qnext or qprev index	*/
#define	MAXKEY	0x7FFFFFFF	/* Max key that can be stored in queue	*/
#define	MINKEY	0x80000000	/* Min key that can be stored in queue	*/

struct	qentry	{		/* One per process plus two per list	*/
	int32	qkey;		/* Key on which the queue is ordered	*/
	qid16	qnext;		/* Index of next process or tail	*/
	qid16	qprev;		/* Index of previous process or head	*/
};

extern	struct qentry	queuetab[];

/* Inline queue manipulation functions */

#define	queuehead(q)	(q)							/* Returns queuehead */
#define	queuetail(q)	((q) + 1)					/* Returns queuetail */
#define	firstid(q)	(queuetab[queuehead(q)].qnext)	/* Returns next element of queuehead */
													/* This is essentially the first element */
#define	lastid(q)	(queuetab[queuetail(q)].qprev)	/* Returns prev element of queuetail */
													/* This is essentially the last element */
#define	isempty(q)	(firstid(q) >= NPROC)			/* Checks first entry for empty */
#define	nonempty(q)	(firstid(q) <  NPROC)			/* Checks first entry for empty */
#define	firstkey(q)	(queuetab[firstid(q)].qkey)		/* Returns key of first element of the queue */
#define	lastkey(q)	(queuetab[ lastid(q)].qkey)		/* Returns key of prev element of the queue*/

/* Inline to check queue id assumes interrupts are disabled */

#define	isbadqid(x)	(((int32)(x) < NPROC) || (int32)(x) >= NQENT-1)