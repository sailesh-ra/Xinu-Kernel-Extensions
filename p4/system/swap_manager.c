//swap_manager.c
#include <xinu.h>

//#define SWAP_CLEANUP_DEBUG

struct swap_entry swap_table[MAX_SWAP_SIZE];
bool8 swap_free[MAX_SWAP_SIZE];
bool8 swap_dual[MAX_SWAP_SIZE];
uint32 swap_count;
unsigned debug_swapping;
int32 swap_clock_hand;

int32 getswapframefromList(pid32 pid, uint32 vpn)
{
    int i;
    for(i = 0; i < MAX_SWAP_SIZE; i++)
    {
        if(swap_table[i].pid == pid && swap_table[i].vpn == vpn)
        {
            return i;
        }
    }
    return -1;
}

void initialize_swap(void) {
    int i;

    /* Mark all swap frames as free */
    for (i = 0; i < MAX_SWAP_SIZE; i++) {
        swap_free[i] = TRUE;          /* Frame is free */
        swap_table[i].pid = -1;       /* No owner */
        swap_table[i].vpn = 0;        /* Clear VPN */
        swap_dual[i] = FALSE;    /* Not dual-mapped */
    }

    /* Reset swap count */
    swap_count = 0;

    swap_clock_hand = -1;

    /* Initialize debug counter (set by test cases, but safe to init) */
    debug_swapping = 0;
}

/*
 * Allocate a swap frame using first-fit policy
 * Params:
 *   - pid: Owner process
 *   - vpn: Virtual page number being swapped
 * Returns Swap frame index (0 to MAX_SWAP_SIZE-1), or -1 if full
 *   - Scan swap_free[] for first free slot
 *   - Mark as used
 *   - Record (pid, vpn) in swap_table
 *   - Increment swap_count
 */
int alloc_swap_frame(pid32 pid, uint32 vpn) {
    intmask mask;
    int i;

    mask = disable();

    for (i = 0; i < MAX_SWAP_SIZE; i++) {
        if (swap_free[i] == TRUE && swap_dual[i] == FALSE) {
            /* Found free frame - allocate it */
            swap_free[i] = FALSE;
            swap_table[i].pid = pid;
            swap_table[i].vpn = vpn;
            swap_count++;

            restore(mask);
            // if(i == 0x2432)
            // {
            //     kprintf("Allocated swap frame 0x2432 to P%d vpn 0x%x\n", pid, vpn);
            // }
            return i;  /* Return swap frame index */
       
        }
    }
    /* First-fit: scan for first free swap frame */
    kprintf("Looking for dual-mapped free swap frame\n");
    for (i = swap_clock_hand; i < MAX_SWAP_SIZE; i++) {
        if (swap_free[i] == TRUE && swap_dual[i] == TRUE) {
            /* Found free frame - allocate it */
            swap_free[i] = FALSE;
            swap_dual[i] = FALSE;
            pid32 owned_pid = swap_table[i].pid;
            uint32 owned_vpn = swap_table[i].vpn;
            pd_t *owner_pdbr = proctab[owned_pid].prpd;

            uint32 saved_cr3 = read_cr3();
            write_cr3((uid32)pd_base_addr);

            virt_addr_t decoded_virt_addr = decode_virtual_address(owned_vpn);
            pd_t page_dir_entry = owner_pdbr[decoded_virt_addr.pd_offset];

            pt_t *page_table_entry_address = (pt_t *)((page_dir_entry.pd_base << 12) + 
                                             ((decoded_virt_addr.pt_offset) * sizeof(pt_t)));
            pt_t page_table_entry = *page_table_entry_address;
            page_table_entry.pt_swap = 0;  /* Clear swapped bit */
            // page_table_entry.pt_dirty = 0; /* Clear dirty bit */
            *page_table_entry_address = page_table_entry;

            write_cr3(saved_cr3);
            swap_clock_hand = (i + 1) % MAX_SWAP_SIZE;
            swap_table[i].pid = pid;
            swap_table[i].vpn = vpn;
            swap_count++;

            restore(mask);
            if(i == 0x2432)
            {
                kprintf("Allocated dual mapped swap frame 0x2432 to P%d vpn 0x%x\n", pid, vpn);
            }
            return i;  /* Return swap frame index */
        }
    }

    /* No free swap frames (should not happen per spec) */
    restore(mask);
    return -1;
}

/*
 * Free a single swap frame
 * Params:  swap_frame index
 *   - Mark swap_free[swap_frame] = TRUE
 *   - Set swap_table[swap_frame].pid = -1
 *   - Decrement swap_count
 */
void free_swap_frame(int swap_frame) {
    intmask mask;

    mask = disable();

    /* Validate swap frame index */
    if (swap_frame < 0 || swap_frame >= MAX_SWAP_SIZE) {
        kprintf("free_swap_frame: Invalid swap frame %d\n", swap_frame);
        restore(mask);
        return;
    }

    /* Check if already free */
    if (swap_free[swap_frame] == TRUE) {
        kprintf("free_swap_frame: Frame %d already free\n", swap_frame);
        restore(mask);
        return;
    }

    /* Free the swap frame */
    swap_free[swap_frame] = TRUE;
    swap_table[swap_frame].pid = -1;
    swap_table[swap_frame].vpn = 0;
    swap_count--;

    restore(mask);
}

/*
 * Free all swap frames owned by a process (on process exit)
 * Params:  pid of exiting process
 *   - Scan swap_table for entries with matching pid
 *   - Free each matching swap frame
 */
void free_process_swap(pid32 pid) {
    intmask mask;
    int i;

    mask = disable();

    #ifdef SWAP_CLEANUP_DEBUG
    kprintf("[free_process_swap] Starting cleanup for P%d\n", pid);
    #endif

    /* Scan all swap frames */
    for (i = 0; i < MAX_SWAP_SIZE; i++) {
        /* Check if this swap frame belongs to the exiting process */
        if (swap_table[i].pid == pid) {
            #ifdef SWAP_CLEANUP_DEBUG
            kprintf("[free_process_swap] Freeing swap frame %d for P%d\n", i, pid);
            #endif

            /* Free this swap frame */
            swap_free[i] = TRUE;
            swap_dual[i] = FALSE;
            swap_table[i].pid = -1;
            swap_table[i].vpn = 0;
            swap_count--;
        }
    }

    #ifdef SWAP_CLEANUP_DEBUG
    kprintf("[free_process_swap] Done cleaning up P%d\n", pid);
    #endif

    restore(mask);
}


int free_swap_pages(void) {
    return MAX_SWAP_SIZE - swap_count;
}