/* lock.h - lock data structures and constants */

#ifndef _LOCK_H_
#define _LOCK_H_

#define LOCK_AVAILABLE -1

/* Spinlock (Part 2) */
/* Park with guard (Part 3)*/
#define NSPINLOCKS  20
#define NLOCKS      20
#define NALOCKS     20
#define NPILOCKS    20

typedef struct {
    uint32  flag;       // 0 = free, 1 = taken
    pid32   owner;
} sl_lock_t;

typedef struct {
    uint32  flag;       // 0 = free, 1 = taken
    uint32  guard;
    pid32   owner;
    qid16   lock_q;
} lock_t;

typedef struct {
    uint32  flag;
    uint32  guard;
    pid32   owner;
    qid16   lock_q;
} al_lock_t;

typedef struct {
    uint32  flag;
    uint32  guard;
    pid32   owner;
    qid16   lock_q;
    uint32  id;
}  pi_lock_t;

pid32 pi_lock_owners_pid[NPILOCKS+1];
pid32 pi_lock_highest_prio_waiting_pid[NPILOCKS+1];

#endif  /* _LOCK_H_ */