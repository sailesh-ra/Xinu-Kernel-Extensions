/* pi_lock.c - pi_initlock, pi_lock, pi_unlock */

#include <xinu.h>

//#define DEBUG

/*------------------------------------------------------------------------
 *  pi_initlock  -  Initialize lock
 *------------------------------------------------------------------------
 */
syscall pi_initlock(
    pi_lock_t *l
)
{
    static uint32 lock_cnt = 0;
    static uint32 lock_id = 0;

    if (++lock_cnt > NPILOCKS)
    {
        return SYSERR;
    }

    l->flag = 0;
    l->guard = 0;
    l->owner = LOCK_AVAILABLE;
    l->lock_q = newqueue();
    l->id = ++lock_id;


    return OK;
}

/*------------------------------------------------------------------------
 *  pi_lock  -  Acquire the lock
 *------------------------------------------------------------------------
 */
syscall pi_lock(
    pi_lock_t *l
)
{
    struct	procent *currptr;
    struct  procent *ownerptr;

    while (test_and_set(&l->guard, 1) == 1) sleepms(QUANTUM);

    if (l->flag == 0) {
        l->flag = 1;
        l->owner = currpid;
        
        pi_lock_owners_pid[l->id] = currpid;
        pi_lock_highest_prio_waiting_pid[l->id] = 0;

        l->guard = 0;
    }
    else {
        /* enqueue in waiting list */
        enqueue(currpid, l->lock_q);

        currptr = &proctab[currpid];
        ownerptr = &proctab[l->owner];

        /* Check if waiting process has higher priority than owner */
        if (currptr->prprio > ownerptr->prprio) {
            kprintf("priority_change=P%d::%d-%d\n", l->owner, ownerptr->prprio, currptr->prprio);
            ownerptr->prprio = currptr->prprio;
        }
        
        if (currptr->prprio > pi_lock_highest_prio_waiting_pid[l->id]) {
            pi_lock_highest_prio_waiting_pid[l->id] = currptr->prprio;
        }
        
        /* park */
        pi_setpark();
        l->guard = 0;
        pi_park();
        
        l->owner = currpid;

        if (pi_lock_highest_prio_waiting_pid[l->id] > proctab[l->owner].prprio) {
            kprintf("priority_change=P%d::%d-%d\n", l->owner, proctab[l->owner].prprio, pi_lock_highest_prio_waiting_pid[l->id]);
            proctab[l->owner].prprio = pi_lock_highest_prio_waiting_pid[l->id];
        }
        
    }
    
    return OK;
}

/*------------------------------------------------------------------------
 *  pi_unlock  -  Release the lock
 *------------------------------------------------------------------------
 */
syscall pi_unlock(
    pi_lock_t *l
)
{
    while (test_and_set(&l->guard, 1) == 1) sleepms(QUANTUM);
    
    /* If the owner holds more than 1 lock */
    int i;
    for (i = 1; i < NPILOCKS+1; i++) {
        if ((pi_lock_owners_pid[i] == l->owner) && (i != l->id)) {
            if ((pi_lock_highest_prio_waiting_pid[i] > proctab[currpid].prOldPrio) && (pi_lock_highest_prio_waiting_pid[i] != proctab[currpid].prprio)) {
                kprintf("priority_change=P%d::%d-%d\n", currpid, proctab[currpid].prprio, pi_lock_highest_prio_waiting_pid[i]);
                proctab[currpid].prprio = pi_lock_highest_prio_waiting_pid[i];
                break;
            }
        }
    }
    l->guard = 0;
    
    /* Reset Priority */
    if (i == NPILOCKS+1) {
        if (proctab[currpid].prprio != proctab[currpid].prOldPrio) {
            kprintf("priority_change=P%d::%d-%d\n", currpid, proctab[currpid].prprio, proctab[currpid].prOldPrio);
            proctab[currpid].prprio = proctab[currpid].prOldPrio;
        }
    }
    
    if (l->owner == currpid) {
        
        if (isempty(l->lock_q)) {
            l->flag = 0;
            l->owner = LOCK_AVAILABLE;
            pi_lock_owners_pid[l->id] = 0;
            pi_lock_highest_prio_waiting_pid[l->id] = 0;
        }
        else {
            pi_unpark(dequeue(l->lock_q));
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

void pi_setpark()
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
void pi_park()
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
void pi_unpark(
    pid32 pid
)
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[pid];
    prptr->prSetpark = 0;

    ready(pid);

    restore(mask);          /* enable interrupts */
}