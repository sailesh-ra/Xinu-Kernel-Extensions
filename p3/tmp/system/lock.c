/* lock.c - initlock, lock, unlock */

#include <xinu.h>

/*------------------------------------------------------------------------
 * initlock  -  Initialize a lock
 *------------------------------------------------------------------------
 */
syscall initlock(lock_t *l)
{
    static uint32 lock_cnt = 0;
    
    if (lock_cnt >= NLOCKS)
    {
        return SYSERR;
    }
    
    l->flag = 0;
    l->guard = 0;
    l->owner = LOCK_AVAILABLE;
    l->lock_q = newqueue();

    lock_cnt++;
    return OK;
}

/*------------------------------------------------------------------------
 * lock  -  Acquire a lock (busy wait)
 *------------------------------------------------------------------------
 */
syscall lock(lock_t *l)
{
    struct procent *prptr;

    // Acquire guard spinlock
    while (test_and_set(&l->guard, 1) == 1) {
        sleepms(QUANTUM);
    }

    // Check if lock is available
    if (l->flag == 0) {
        // Lock is free, acquire it
        l->flag = 1;
        l->owner = currpid;
        l->guard = 0;  // Release guard
    }
    else {
        // Lock is held, park this process
        enqueue(currpid, l->lock_q);
        prptr = &proctab[currpid];
        prptr->prSetpark = 1;
        l->guard = 0;  // MUST release guard before parking!
        park();        // This will reschedule
        // When unparked, lock is transferred to us
        l->owner = currpid;
    }

    return OK;
}

/*------------------------------------------------------------------------
 * unlock  -  Release a lock
 *------------------------------------------------------------------------
 */
syscall unlock(
    lock_t *l
)
{
    while (test_and_set(&l->guard, 1) == 1) sleepms(QUANTUM);

    if (l->owner == currpid) {
        if (isempty(l->lock_q)) {
            l->flag = 0;
            l->owner = LOCK_AVAILABLE;
        }
        else {
            unpark(dequeue(l->lock_q));
        }
        l->guard = 0;

        return OK;
    }
    else {
        l->guard = 0;

        return SYSERR;
    }
}

/*------------------------------------------------------------------------
 *  setpark  -  Mark that a process is going to park
 *------------------------------------------------------------------------
 */
void setpark()
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[currpid];
    prptr->prSetpark = 1;

    restore(mask);          /* enable interrupts */
}

/*------------------------------------------------------------------------
 *  park  -  Park a process
 *------------------------------------------------------------------------
 */
void park()
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[currpid];

    if (prptr->prSetpark == 1) {
        prptr->prstate = PR_WAIT;
        prptr->prSetpark = 0;
        resched();
    }
    
    restore(mask);          /* enable interrupts */
}

/*------------------------------------------------------------------------
 *  unpark  -  Unpark a process
 *------------------------------------------------------------------------
 */
void unpark(pid32 pid)
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[pid];
    prptr->prSetpark = 0;

    ready(pid);

    restore(mask);          /* enable interrupts */
}