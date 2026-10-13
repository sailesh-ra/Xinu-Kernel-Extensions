/* spinlock.c - sl_initlock, sl_lock, sl_unlock */

#include <xinu.h>

/*------------------------------------------------------------------------
 * sl_initlock  -  Initialize a spinlock
 *------------------------------------------------------------------------
 */
syscall sl_initlock(sl_lock_t *l)
{
    static uint32 lock_cnt = 0;
    
    if (lock_cnt > NSPINLOCKS)
    {
        return SYSERR;
    }
    
    l->flag = 0;
    l->owner = LOCK_AVAILABLE;

    lock_cnt++;

    return OK;
}

/*------------------------------------------------------------------------
 * sl_lock  -  Acquire a spinlock (busy wait)
 *------------------------------------------------------------------------
 */
syscall sl_lock(sl_lock_t *l)
{
    while (test_and_set(&l->flag, 1) == 1) {
        ; // spin
    }
    l->owner = currpid;
    return OK;
}

/*------------------------------------------------------------------------
 * sl_unlock  -  Release a spinlock
 *------------------------------------------------------------------------
 */
syscall sl_unlock(sl_lock_t *l)
{
        if (l->owner == currpid) {
        l->flag = 0;
        l->owner = LOCK_AVAILABLE;
        return OK;
    }
    else {
        return SYSERR;
    }
}