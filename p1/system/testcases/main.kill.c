#include <xinu.h>

void test_process() {
    while (TRUE) {
        sleep(1);  // Keep the process alive for testing
    }
}

int main() {
    pid32 p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15;

    /* Create a tree of user processes */
    p1 = create(test_process, INITSTK, 20, "proc1", 0);
    proctab[p1].user_process = TRUE;
    kprintf("Process p1 has PID: %d\n", p1);

    p2 = create(test_process, INITSTK, 20, "proc2", 0);
    proctab[p2].user_process = TRUE;
    proctab[p2].prparent = p1;  /* p1 is the parent of p2 */
    kprintf("Process p2 has PID: %d\n", p2);

    p3 = create(test_process, INITSTK, 20, "proc3", 0);
    proctab[p3].user_process = TRUE;
    proctab[p3].prparent = p1;  /* p1 is the parent of p3 */
    kprintf("Process p3 has PID: %d\n", p3);

    p4 = create(test_process, INITSTK, 20, "proc4", 0);
    proctab[p4].user_process = TRUE;
    proctab[p4].prparent = p2;  /* p2 is the parent of p4 */
    kprintf("Process p4 has PID: %d\n", p4);

    p5 = create(test_process, INITSTK, 20, "proc5", 0);
    proctab[p5].user_process = TRUE;
    proctab[p5].prparent = p2;  /* p2 is the parent of p5 */
    kprintf("Process p5 has PID: %d\n", p5);

    p6 = create(test_process, INITSTK, 20, "proc6", 0);
    proctab[p6].user_process = TRUE;
    proctab[p6].prparent = p3;  /* p3 is the parent of p6 */
    kprintf("Process p6 has PID: %d\n", p6);

    p7 = create(test_process, INITSTK, 20, "proc7", 0);
    proctab[p7].user_process = TRUE;
    proctab[p7].prparent = p3;  /* p3 is the parent of p7 */
    kprintf("Process p7 has PID: %d\n", p7);

    /* p4's children */
    p8 = create(test_process, INITSTK, 20, "proc8", 0);
    proctab[p8].user_process = TRUE;
    proctab[p8].prparent = p4;  /* p4 is the parent of p8 */
    kprintf("Process p8 has PID: %d\n", p8);

    p9 = create(test_process, INITSTK, 20, "proc9", 0);
    proctab[p9].user_process = TRUE;
    proctab[p9].prparent = p4;  /* p4 is the parent of p9 */
    kprintf("Process p9 has PID: %d\n", p9);

    /* p5's children */
    p10 = create(test_process, INITSTK, 20, "proc10", 0);
    proctab[p10].user_process = TRUE;
    proctab[p10].prparent = p5;  /* p5 is the parent of p10 */
    kprintf("Process p10 has PID: %d\n", p10);

    p11 = create(test_process, INITSTK, 20, "proc11", 0);
    proctab[p11].user_process = TRUE;
    proctab[p11].prparent = p5;  /* p5 is the parent of p11 */
    kprintf("Process p11 has PID: %d\n", p11);

    /* p10's children */
    p12 = create(test_process, INITSTK, 20, "proc12", 0);
    proctab[p12].user_process = TRUE;
    proctab[p12].prparent = p10;  /* p10 is the parent of p12 */
    kprintf("Process p12 has PID: %d\n", p12);

    p13 = create(test_process, INITSTK, 20, "proc13", 0);
    proctab[p13].user_process = TRUE;
    proctab[p13].prparent = p10;  /* p10 is the parent of p13 */
    kprintf("Process p13 has PID: %d\n", p13);

    /* p12's children */
    p14 = create(test_process, INITSTK, 20, "proc14", 0);
    proctab[p14].user_process = TRUE;
    proctab[p14].prparent = p12;  /* p12 is the parent of p14 */
    kprintf("Process p14 has PID: %d\n", p14);

    p15 = create(test_process, INITSTK, 20, "proc15", 0);
    proctab[p15].user_process = TRUE;
    proctab[p15].prparent = p12;  /* p12 is the parent of p15 */
    kprintf("Process p15 has PID: %d\n", p15);

    /* Print active processes before killing */
    kprintf("Before killing process %d\n", p2);
    print_active_processes();

    /* Kill a parent process (e.g., p2) and trigger cascading termination */
    kprintf("Killing process %d\n", p2);
    kill(p2);  /* This should also terminate p4,p5,p8,p9,p10, p11, p12, p13, p14, p15 */

    /* Print active processes after killing */
    kprintf("After killing process %d\n", p2);
    print_active_processes();

    return OK;
}

/* Function to print active processes and their children */
void print_active_processes() {
    pid32 i, child;
    for (i = 0; i < NPROC; i++) {
        if (proctab[i].prstate != PR_FREE) {
            kprintf("Process %d (Parent: %d) is active. Children: ", i, proctab[i].prparent);
            for (child = 0; child < NPROC; child++) {
                if (proctab[child].prparent == i && proctab[child].prstate != PR_FREE) {
                    kprintf("%d ", child);  /* Print child process ID */
                }
            }
            kprintf("\n");
        }
    }
}
