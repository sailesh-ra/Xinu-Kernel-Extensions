/* insert.c - insert */

#include <xinu.h>

/*------------------------------------------------------------------------
 *  insert  -  Insert a process into a queue in descending key order
 *------------------------------------------------------------------------
 */
status	insert(
	  pid32		pid,		/* ID of process to insert	*/
	  qid16		q,		/* ID of queue to use		*/
	  int32		key		/* Key for the inserted process	*/
	)
{
	qid16	curr;			/* Runs through items in a queue*/
	qid16	prev;			/* Holds previous node index	*/

	if (isbadqid(q) || isbadpid(pid)) {
		return SYSERR;
	}

	curr = firstid(q);
	while (queuetab[curr].qkey >= key) {
		curr = queuetab[curr].qnext;
	}

	/* Insert process between curr node and previous node */

	prev = queuetab[curr].qprev;	/* Get index of previous node	*/
	queuetab[pid].qnext = curr;
	queuetab[pid].qprev = prev;
	queuetab[pid].qkey = key;
	queuetab[prev].qnext = pid;
	queuetab[curr].qprev = pid;
	return OK;
}

status	insert_to_user(
	  pid32		pid,		/* ID of process to insert	*/
	  qid16		q,		/* ID of queue to use		*/
	  int32		key		/* Key for the inserted process	*/
	)
{
    qid16    curr;            /* Runs through items in a queue*/
    qid16    prev;            /* Holds previous node index    */

    if (isbadpid(pid)) {
        return SYSERR;
    }

    curr = firstid(q);
    if(isempty(q) == TRUE) 
	{
		queuetab[pid].qkey = key;
        enqueue(pid, q);
	}
    else 
	{
        while (queuetab[curr].qkey > key) {
            curr = queuetab[curr].qnext;
        }
        while((queuetab[curr].qkey == key) && (pid > curr)) {
            curr = queuetab[curr].qnext;
        }

        /* Insert process between curr node and previous node */

        prev = queuetab[curr].qprev;    /* Get index of previous node    */
        queuetab[pid].qnext = curr;
        queuetab[pid].qprev = prev;
        queuetab[pid].qkey = key;
        queuetab[prev].qnext = pid;
        queuetab[curr].qprev = pid;
    }
    return OK;
}
