/* fork.c - fork, creates a new process as a copy of the current one */

#include <xinu.h>

pid32 fork(void)
{
    struct procent *parent_proc;        /* Pointer to the parent process */
    struct procent *child_proc;         /* Pointer to the child process */
    uint32 stack_size;                  /* Stack size to allocate for child */
    uint32 saved_sp, *stack_ptr;        /* Stack pointers for copying stack */
    uint32 *child_stack_base;           /* Stack base for the child */
    pid32 child_pid;                    /* Process ID for the new child process */
    intmask interrupt_mask;             /* Interrupt mask for disabling interrupts */
    int32 i;                            /* Loop iterator */
    unsigned long *parent_ebp, *parent_sp; /* Used for accessing parent's EBP and ESP */

    /* Get pointer to current parent process */
    parent_proc = &proctab[currpid];
    stack_size = parent_proc->prstklen; /* Set stack size to parent's stack size */

    /* Disable interrupts to safely create the child process */
    interrupt_mask = disable();

    /* Validate and allocate stack for the child */
    if (stack_size < MINSTK)
        stack_size = MINSTK;
    stack_size = (uint32) roundmb(stack_size);

    if ((parent_proc->prprio < 1) || 
        ((child_pid = newpid()) == SYSERR) || 
        ((child_stack_base = (uint32 *)getstk(stack_size)) == (uint32 *)SYSERR)) {
        restore(interrupt_mask);
        return SYSERR;
    }

    /* Increment the process count */
    prcount++;

    /* Set up the child process control block (PCB) */
    child_proc = &proctab[child_pid];
    child_proc->prstate = PR_SUSP;                     /* Child starts suspended */
    child_proc->prprio = parent_proc->prprio;          /* Inherit parent's priority */
    child_proc->prstklen = stack_size;                 /* Inherit stack size */
    child_proc->prstkbase = (char *)child_stack_base;  /* Set base of the child's stack */
    child_proc->prparent = currpid;                    /* Set parent PID */
    child_proc->prhasmsg = FALSE;                      /* No initial messages */
    child_proc->user_process = parent_proc->user_process; /* Inherit process type (user/system) */

    /* Copy parent process name */
    for (i = 0; i < PNMLEN - 1 && (child_proc->prname[i] = parent_proc->prname[i]) != NULLCH; i++);
    child_proc->prname[PNMLEN - 1] = NULLCH;

    /* Copy parent's file descriptors */
    for (i = 0; i < NDESC; i++) {
        child_proc->prdesc[i] = parent_proc->prdesc[i];
    }

    /* Copy stack content from parent to child */
    saved_sp = (uint32)child_stack_base; /* Save child's stack base */

    asm("movl %%ebp, %0\n" : "=r"(parent_ebp)); /* Get parent's EBP */
    parent_sp = (unsigned long *)(parent_proc->prstkbase); /* Get parent's stack base */

    /* Copy the stack from parent to child */
    while (parent_sp >= parent_ebp) {
        *child_stack_base = *parent_sp;
        parent_sp--;
        child_stack_base--;
    }

    /* Set up child stack for context switch */
    *child_stack_base = 0x00000200;  /* Set child process to run with interrupts enabled */

    /* Set up registers for the child */
    *--child_stack_base = 0;         /* %eax */
    *--child_stack_base = 0;         /* %ecx */
    *--child_stack_base = 0;         /* %edx */
    *--child_stack_base = 0;         /* %ebx */
    *--child_stack_base = 0;         /* %esp (will be set later) */
    stack_ptr = child_stack_base;    /* Save stack location */
    *--child_stack_base = saved_sp;  /* %ebp */
    *--child_stack_base = 0;         /* %esi */
    *--child_stack_base = 0;         /* %edi */
    *stack_ptr = (unsigned long)(child_proc->prstkptr = (char *)child_stack_base);

    /* Set child to ready state and insert it into the ready queue */
    child_proc->prstate = PR_READY;
    insert(child_pid, readylist, child_proc->prprio);

    /* Restore interrupts */
    restore(interrupt_mask);

    /* Return process ID */
    if (currpid == child_pid) {
        return NPROC; /* Child process returns */
    } else {
        return child_pid; /* Parent process gets child PID */
    }
}
