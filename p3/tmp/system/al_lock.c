/* active_lock.c - al_initlock, al_lock, al_unlock, al_trylock */

#include <xinu.h>

//#define DEBUG

/*------------------------------------------------------------------------
 *  al_initlock  -  Initialize active lock
 *------------------------------------------------------------------------
 */
syscall al_initlock(
    al_lock_t *l
)
{
    static uint32 lock_cnt = 0;
    

    if (lock_cnt > NLOCKS)
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
 *  al_lock  -  Acquire the active lock
 *------------------------------------------------------------------------
 */
syscall al_lock(
    al_lock_t *l
)
{
    struct	procent *prptr;
    
    /* Capture the guard */
    while (test_and_set(&l->guard, 1) == 1);
    
    /* Capture the flag, if possible */
    if(l->flag == 0) {
        l->flag = 1;
        l->owner = currpid;
        l->guard = 0;
    }
    /* If not able to capture the lock, enqueue to lock queue and wait */
    else {
        /* update pcb params*/
        prptr = &proctab[currpid];
        prptr->prLockWaitingOn = l->owner;
        prptr->prSetpark = 1;

        /* Check for deadlocks */
        detect_deadlock(l);

        /* enqueue in waiting list */
        enqueue(currpid, l->lock_q);

        /* park */
        al_setpark();
        l->guard = 0;
        al_park();
    }

    return OK;
}

/*------------------------------------------------------------------------
 *  al_unlock  -  Release the active lock
 *------------------------------------------------------------------------
 */
syscall al_unlock(
    al_lock_t *l
)
{
    struct	procent *prptr;

    /* Capture the guard */
    while (test_and_set(&l->guard, 1) == 1);

    if (l->owner == currpid) {
        if (isempty(l->lock_q)) {
            l->flag = 0;
            l->owner = LOCK_AVAILABLE;
        }
        else {
            prptr = &proctab[currpid];
            prptr->prLockWaitingOn = -1;
            al_unpark(dequeue(l->lock_q));
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
 *  al_trylock  -  Checks if lock can be obtained
 *------------------------------------------------------------------------
 */
bool8 al_trylock(
    al_lock_t *l
)
{
    /* Capture the guard */
    while (test_and_set(&l->guard, 0) == 1);

    /* If lock is available, capture it and return true */
    if (l->flag == 0) {
        l->flag = 1;
        l->owner = currpid;
        l->guard = 0;

        return TRUE;
    }
    /* Id lock is not available, return false */
    else {
        l->guard = 0;

        return FALSE;
    }
}   

/*------------------------------------------------------------------------
 *  al_setpark  -  Mark that a process is going to al_park
 *------------------------------------------------------------------------
 */
void al_setpark()
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[currpid];
    prptr->prSetpark = 1;

    restore(mask);          /* enable interrupts */
}

/*------------------------------------------------------------------------
 *  al_park  -  Park a process
 *------------------------------------------------------------------------
 */
void al_park()
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
 *  al_unpark  -  Unpark a process
 *------------------------------------------------------------------------
 */
void al_unpark(
    pid32 pid
)
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct	procent *prptr;
    prptr = &proctab[currpid];
    prptr->prSetpark = 0;

    ready(pid);

    restore(mask);          /* enable interrupts */
}

/*------------------------------------------------------------------------------------------
 *  detect_deadlock  -  Detects deadlocks and prints the chain of pids leading to deadlock
 *------------------------------------------------------------------------------------------
 */
void detect_deadlock(
    al_lock_t *l
)
{
    intmask 	mask;    	/* Interrupt mask */
    mask = disable();       /* disable interrupts for atomic-like operation */

    struct  procent *prptr;
    pid32 check_pid = l->owner;
    uint32 deadlocked_pids_chain[NALOCKS];
    uint32 deadlocked_pids_chain_len = 0;
    bool8  deadlock_detected = FALSE;
    bool8  currpid_in_original_cycle = FALSE;
    uint32 i = 0;
    
#ifdef DEBUG
    /* DEBUG: Show who's calling detect_deadlock */
    kprintf("\n[DEBUG] P%d checking, lock owner=P%d", currpid, l->owner);
#endif
    
    /* Initialize the deadlock chain with invalid PID */
    for (i = 0; i < NALOCKS; i++)
	{
		deadlocked_pids_chain[i] = NPROC + 1;
	}

    /* Check for circular dependencies */

    while ((check_pid != -1))
    {
        prptr = &proctab[check_pid];
        
#ifdef DEBUG
        kprintf("\n[DEBUG] P%d: following chain, check_pid=P%d, waiting_on=P%d", 
                currpid, check_pid, prptr->prLockWaitingOn);
#endif
        
        if (prptr->prLockWaitingOn == l->owner) {
            deadlocked_pids_chain[deadlocked_pids_chain_len++] = check_pid;
            deadlock_detected = TRUE;
#ifdef DEBUG
            kprintf("\n[DEBUG] P%d: DEADLOCK DETECTED!", currpid);
#endif
            break;
        }

        /* Add the current process to the deadlock chain if unique */
        bool8 is_unique = TRUE;
        uint32 j;
        for (j = 0; j < deadlocked_pids_chain_len; j++) {
            if (deadlocked_pids_chain[j] == check_pid) {
                is_unique = FALSE;
                break;
            }
        }

        if (is_unique) {
            deadlocked_pids_chain[deadlocked_pids_chain_len++] = check_pid;
        }

        /* Check if we found currpid during traversal */
        if (check_pid == currpid) {
            currpid_in_original_cycle = TRUE;
        }

        check_pid = prptr->prLockWaitingOn;
    }

    /* Add currpid to the chain if unique (it's part of the deadlock!) */
    if (deadlock_detected) {
    #ifdef DEBUG
        kprintf("\n[DEBUG] P%d: Adding currpid to chain", currpid);
    #endif
        bool8 currpid_unique = TRUE;
        uint32 k;
        for (k = 0; k < deadlocked_pids_chain_len; k++) {
            if (deadlocked_pids_chain[k] == currpid) {
                currpid_unique = FALSE;
    #ifdef DEBUG
                kprintf("\n[DEBUG] P%d: currpid already in chain", currpid);
    #endif
                currpid_in_original_cycle = TRUE;  /* Already in chain = part of original cycle */
                break;
            }
        }
        if (currpid_unique) {
            deadlocked_pids_chain[deadlocked_pids_chain_len++] = currpid;
            /* currpid_in_original_cycle stays FALSE - this is a latecomer */
    #ifdef DEBUG
            kprintf("\n[DEBUG] P%d: currpid added to chain", currpid);
    #endif
        }
    }

    /* Sort the deadlock chain if a deadlock was detected */
    if (deadlock_detected) {
        uint32 i;
        for (i = 0; i < deadlocked_pids_chain_len - 1; i++) {
            uint32 j;
            for (j = 0; j < deadlocked_pids_chain_len - i - 1; j++) {
                if (deadlocked_pids_chain[j] > deadlocked_pids_chain[j + 1]) {
                    uint32 temp = deadlocked_pids_chain[j];
                    deadlocked_pids_chain[j] = deadlocked_pids_chain[j + 1];
                    deadlocked_pids_chain[j + 1] = temp;
                }
            }
        }

    #ifdef DEBUG
        kprintf("\n[DEBUG] P%d: chain sorted, first=P%d, currpid=%d, match=%d", 
                currpid, deadlocked_pids_chain[0], currpid, 
                (currpid == deadlocked_pids_chain[0]));
    #endif

        /* Only print if currpid is the smallest in the chain */
        if (currpid_in_original_cycle) {
            /* Print detected deadlock chain */
            kprintf("\ndeadlock_detected=");
            uint32 j;
            for (j = 0; j < deadlocked_pids_chain_len; j++) {
                kprintf("P%d", deadlocked_pids_chain[j]);
                if (j < deadlocked_pids_chain_len - 1) {
                    kprintf("-");
                }
            }
            kprintf("\n");
        }
    #ifdef DEBUG
        else {
            kprintf("\n[DEBUG] P%d: NOT printing (not smallest)", currpid);
        }
        #endif
    }
    #ifdef DEBUG
    else {
        kprintf("\n[DEBUG] P%d: No deadlock detected", currpid);
    }
    #endif

    restore(mask);          /* enable interrupts */
}