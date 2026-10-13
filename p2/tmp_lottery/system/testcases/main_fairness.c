#include <xinu.h>
#include <stdio.h>

void timed_execution(uint32 runtime){
    while(proctab[currpid].runtime < runtime);
}

int main() {
    pid32 prA, prB;
    uint32 runtime;
    uint32 turnaround_A, turnaround_B;
    double fairness;
	int i;
    
    kprintf("\n=== Lottery Scheduling Fairness Test ===\n\n");
    
    // Test with different runtime values
    uint32 runtimes[] = {100, 200, 500, 1000, 2000, 5000};
    
    for (i = 0; i < 6; i++) {
        runtime = runtimes[i];
        
        // Create two processes with same tickets
        prA = create_user_process(timed_execution, 1024, "procA", 1, runtime);
        prB = create_user_process(timed_execution, 1024, "procB", 1, runtime);
        
        set_tickets(prA, 100);  // Same tickets
        set_tickets(prB, 100);  // Same tickets
        
        resume(prA);
        resume(prB);
        
        receive();  // Wait for both to finish
        receive();
        
        sleepms(50);
        
        turnaround_A = proctab[prA].turnaroundtime;
        turnaround_B = proctab[prB].turnaroundtime;
        
        // Calculate fairness (smaller / larger)
        if (turnaround_A < turnaround_B) {
            fairness = (double)turnaround_A / turnaround_B;
        } else {
            fairness = (double)turnaround_B / turnaround_A;
        }
        
        kprintf("Runtime=%d: TurnaroundA=%d, TurnaroundB=%d, Fairness=%.3f\n", 
                runtime, turnaround_A, turnaround_B, fairness);
    }
    
    return OK;
}