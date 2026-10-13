/*
 * Free Frame Space (FFS) Manager
 * Manages memory allocation for the Free Frame Space (FFS). It maintains
 * a linked list of free memory blocks (frames) and provides functions to initialize
 * the free list, allocate a frame, and free a frame.
 *
 */

#include <xinu.h>

/* Debug control */
//#define FREEFRAME_DEBUG
// #define DEBUG_FFS
//#define FFS_CLEANUP_DEBUG


struct memblk ffs_memlist;
struct ffs_entry ffs_table[MAX_FFS_SIZE];
uint32 clock_hand = 0;
int ffs_count = 0;


void initialize_ffs() {
    uint32 ffs_start_page = XINU_PAGES + MAX_PT_SIZE;

    struct memblk* memptr = ffs_memlist.mnext = (struct memblk*)(ffs_start_page * PAGE_SIZE);
    memptr->mnext = NULL;
    memptr->mlength = MAX_FFS_SIZE * PAGE_SIZE;
    ffs_memlist.mlength = MAX_FFS_SIZE * PAGE_SIZE; // Used to track the overall available ffs size

    int i;
    for (i = 0; i < MAX_FFS_SIZE; i++) {
        ffs_table[i].pid = -1;
        ffs_table[i].vpn = 0;
        ffs_table[i].used = FALSE;
    }
    clock_hand = 0;
    ffs_count = 0;
}


uint32* getFrame() {

    struct	memblk	*prev, *curr, *leftover;
    intmask	mask;			
    mask = disable();
    prev = &ffs_memlist;
	curr = ffs_memlist.mnext;

    if (curr == NULL) {
        restore(mask);
        return (uint32 *)SYSERR;
    }

    // Allocate one page (4096 bytes)
    if (curr->mlength == PAGE_SIZE) {
        prev->mnext = curr->mnext;
        ffs_memlist.mlength -= PAGE_SIZE;
        restore(mask);
        return (uint32 *)(curr);
    } else if (curr->mlength > PAGE_SIZE) {
        leftover = (struct memblk *)((uint32) curr + PAGE_SIZE);
        prev->mnext = leftover;
        leftover->mnext = curr->mnext;
        leftover->mlength = curr->mlength - PAGE_SIZE;
        ffs_memlist.mlength -= PAGE_SIZE;
        restore(mask);
        return (uint32 *)(curr);
    } else {
        kprintf("FFS Manager : Something Seriously wrong. This should not happen");
        restore(mask);
        return (uint32 *)SYSERR;
    }

}

/*
 * Allocate FFS frame with ownership tracking
 * Params:
 *   - pid: Owner process
 *   - vpn: Virtual page number
 * Returns: Physical address of allocated frame
 *   - Find free frame OR trigger eviction
 *   - Record (pid, vpn) in ffs_table
 *   - Increment ffs_count
 *   - Return frame address
 */
uint32* getFrame_tracked(pid32 pid, uint32 vpn) {
    intmask mask;
    uint32 *frame;
    int frame_index;

    mask = disable();

        #ifdef DEBUG_FFS
        kprintf("[getFrame_tracked] FFS_BASE_PAGE=%d, base_addr=0x%08x\n", 
                FFS_BASE_PAGE, FFS_BASE_PAGE * PAGE_SIZE);
        #endif

    /* Try normal allocation first */
    frame = getFrame();

    #ifdef DEBUG_FFS
    kprintf("[getFrame_tracked] frame=0x%08x\n", frame);
    #endif
    
    if (frame == (uint32 *)SYSERR) {
        /* FFS full - need to evict */
        // kprintf("[getFrame_tracked] FFS full, evicting frame...\n");
        uint32 victim = find_victim_frame();
        evict_frame(victim);
        frame = getFrame();
    }

    /* Track ownership */
    frame_index = ADDR_TO_FFS_INDEX(frame);

    #ifdef DEBUG_FFS
    kprintf("[getFrame_tracked] frame_index=%d\n", frame_index);
    #endif

    ffs_table[frame_index].pid = pid;
    ffs_table[frame_index].vpn = vpn;
    ffs_table[frame_index].used = TRUE;
    ffs_count++;

    restore(mask);
    return frame;
}

/*------------------------------------------------------------------------
 * freeFrame - Free an FFS frame back to the free list (with coalescing)
 *------------------------------------------------------------------------
 */
void freeFrame(uint32* frame_addr) {
    intmask mask;
    struct memblk *prev, *curr, *block;
    uint32 top;
    
    mask = disable();
    
    #ifdef FREEFRAME_DEBUG
    kprintf("[freeFrame] Freeing frame at 0x%08x\n", frame_addr);
    #endif
    
    block = (struct memblk *)frame_addr;
    
    /* Walk list to find insertion point (address-ordered) */
    prev = &ffs_memlist;
    curr = ffs_memlist.mnext;
    
    while (curr != NULL && curr < block) {
        prev = curr;
        curr = curr->mnext;
    }
    
    /* Calculate top of previous block */
    if (prev == &ffs_memlist) {
        top = 0;
    } else {
        top = (uint32)prev + prev->mlength;
    }
    
    #ifdef FREEFRAME_DEBUG
    kprintf("[freeFrame] prev=0x%08x, curr=0x%08x, top=0x%08x\n", prev, curr, top);
    #endif
    
    /* Coalesce with previous block if adjacent */
    if (prev != &ffs_memlist && top == (uint32)block) {
        #ifdef FREEFRAME_DEBUG
        kprintf("[freeFrame] Coalescing with previous block\n");
        #endif
        prev->mlength += PAGE_SIZE;
        block = prev;
    } else {
        /* Insert new node */
        #ifdef FREEFRAME_DEBUG
        kprintf("[freeFrame] Inserting new node\n");
        #endif
        block->mnext = curr;
        block->mlength = PAGE_SIZE;
        prev->mnext = block;
    }
    
    /* Coalesce with next block if adjacent */
    if (curr != NULL && ((uint32)block + block->mlength) == (uint32)curr) {
        #ifdef FREEFRAME_DEBUG
        kprintf("[freeFrame] Coalescing with next block\n");
        #endif
        block->mlength += curr->mlength;
        block->mnext = curr->mnext;
    }
    
    /* Update total free memory */
    ffs_memlist.mlength += PAGE_SIZE;
    
    #ifdef FREEFRAME_DEBUG
    kprintf("[freeFrame] Done. Total free FFS: %d bytes\n", ffs_memlist.mlength);
    #endif
    
    restore(mask);
}

/*
 * Find frame to evict using clock (approximate LRU) algorithm
 * Returns: FFS frame index of victim
 *   - Start from clock_hand position
 *   - For each frame:
 *     - Get owner (pid, vpn) from ffs_table
 *     - Find owner's PTE
 *     - If accessed bit set: clear it, advance clock_hand
 *     - If accessed bit clear: return this frame as victim
 *   - Advance clock_hand after selection
 */
int find_victim_frame(void) {
    intmask mask;
    pid32 owner_pid;
    uint32 vpn;
    uint32 pd_index, pt_index;
    pd_t *pd;
    pt_t *pt;
    pt_t *pte;
    uint32 saved_cr3;
    int victim;

    mask = disable();

    /* Save current CR3 */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);

    while (1) {
        /* Get owner info from ffs_table */

        owner_pid = ffs_table[clock_hand].pid;
        vpn = ffs_table[clock_hand].vpn;

        /* Skip if frame is not in use (shouldn't happen if FFS is full) */
        if (owner_pid == -1 || !ffs_table[clock_hand].used) {
            clock_hand = (clock_hand + 1) % MAX_FFS_SIZE;
            continue;
        }

        pd = proctab[owner_pid].prpd;
        // write_cr3((uint32)pd);

        /* Calculate PD and PT indices from VPN */
        pd_index = (vpn >> 10) & 0x3FF;   /* Upper 10 bits of VPN */
        pt_index = vpn & 0x3FF;            /* Lower 10 bits of VPN */

        pt = (pt_t *)(pd[pd_index].pd_base << 12);

        /* Get the PTE */
        pte = &pt[pt_index];

        if (pte->pt_acc == 1) {
            /* Recently used - give second chance */
            pte->pt_acc = 0;  /* Clear accessed bit */
            clock_hand = (clock_hand + 1) % MAX_FFS_SIZE;
            /* Continue scanning */
        } else {
            /* Not recently used - evict this one */
            victim = clock_hand;
            clock_hand = (clock_hand + 1) % MAX_FFS_SIZE;

            /* Restore original CR3 */
            write_cr3(saved_cr3);
            restore(mask);

            return victim;
        }
    }

    /* Should never reach here */
    write_cr3(saved_cr3);
    restore(mask);
    return -1;
}

/*
   Evict a frame to swap space
 * Params:  FFS frame index to evict
 * Actions:
 *   1. Get owner (pid, vpn) from ffs_table
 *   2. Find owner's PTE
 *   3. Check if already has swap backing (pt_swap == 1)
 *      - If yes: reuse existing swap frame (no copy)
 *      - If no: allocate new swap frame (copy)
 *   4. Update PTE: pt_pres=0, pt_swap=1, pt_base=swap_frame
 *   5. Print debug message if DEBUG_SWAPPING defined
 *   6. Clear ffs_table entry
 *   7. Decrement ffs_count
 */
void evict_frame(int frame_index) {
    intmask mask;
    pid32 owner_pid;
    uint32 vpn;
    uint32 pd_index, pt_index;
    pd_t *pd;
    pt_t *pt;
    pt_t *pte;
    uint32 saved_cr3;
    int swap_frame;
    bool8 needs_copy;

    mask = disable();

    /* 1. Get owner info from ffs_table */
    owner_pid = ffs_table[frame_index].pid;
    vpn = ffs_table[frame_index].vpn;

    /* Save current CR3 */
    saved_cr3 = read_cr3();

    /* 2. Switch to kernel's page directory to access the owners PTE */
    pd = proctab[owner_pid].prpd;
    write_cr3((uint32)pd_base_addr);

    /* Calculate PD and PT indices from VPN */
    pd_index = (vpn >> 10) & 0x3FF;   /* Upper 10 bits of VPN */
    pt_index = vpn & 0x3FF;            /* Lower 10 bits of VPN */

    /* Get page table from page directory */
    pt = (pt_t *)(pd[pd_index].pd_base << 12);

    /* Get the PTE */
    pte = &pt[pt_index];

    /* 3. Check if already has swap backing */
    if (pte->pt_swap == 1 
        // && pte->pt_dirty == 0
    ) {
        swap_frame = getswapframefromList(owner_pid, vpn);
        needs_copy = FALSE;
    } else {
        swap_frame = alloc_swap_frame(owner_pid, vpn);
        needs_copy = TRUE;
        // pte->pt_dirty = 0;  /* Clear dirty bit after eviction */
    }
    freeFrame(FFS_INDEX_TO_ADDR(frame_index));
    /* 4. Update PTE */
    pte->pt_pres = 0;           /* No longer in physical memory */
    pte->pt_swap = 1;           /* Mark as swapped */ 
    pte->pt_base = swap_frame;  /* Store swap frame number */
    pte->pt_acc = 0;            /* Clear accessed bit */

    /* Restore original CR3 */
    write_cr3(saved_cr3);

    /* 5. Print debug message if DEBUG_SWAPPING defined */
#ifdef DEBUG_SWAPPING
    if (debug_swapping > 0) {
        if (needs_copy ) {
            kprintf("eviction:: FFS frame 0x%x, swap frame 0x%x copy\n", 
                    frame_index, swap_frame);
        } else {
            kprintf("eviction:: FFS frame 0x%x, swap frame 0x%x\n", 
                    frame_index, swap_frame);
        }
        debug_swapping--;
    }
#endif

    /* 6. Clear ffs_table entry */
    ffs_table[frame_index].pid = -1;
    ffs_table[frame_index].vpn = 0;
    ffs_table[frame_index].used = FALSE;

    /* 7. Decrement ffs_count */
    ffs_count--;

    restore(mask);
    }

/*
 * Free FFS frame with tracking update
 * Params:
 *   - frame_addr: Physical address of frame
 *   - pid: Owner process (for validation)
 * Actions:
 *   - Convert address to frame index
 *   - Clear ffs_table entry
 *   - Add frame back to free pool
 *   - Decrement ffs_count
 */
void freeFrame_tracked(uint32* frame_addr, pid32 pid) {
    intmask mask;
    int frame_index;
    uint32 ffs_base_addr;

    mask = disable();

    /* Calculate FFS base address */
    ffs_base_addr = (XINU_PAGES + MAX_PT_SIZE) * PAGE_SIZE;

    /* Convert physical address to frame index */
    frame_index = ADDR_TO_FFS_INDEX(frame_addr);

    /* Validate frame index */
    if (frame_index < 0 || frame_index >= MAX_FFS_SIZE) {
        kprintf("freeFrame_tracked: Invalid frame address 0x%x\n", frame_addr);
        restore(mask);
        return;
    }

    if (ffs_table[frame_index].pid != pid) {
        kprintf("freeFrame_tracked: Frame 0x%x not owned by pid %d (owner: %d)\n",
                frame_index, pid, ffs_table[frame_index].pid);
        restore(mask);
        return;
    }

    /* Validate frame is actually in use */
    if (!ffs_table[frame_index].used) {
        kprintf("freeFrame_tracked: Frame 0x%x already free\n", frame_index);
        restore(mask);
        return;
    }

    /* Clear ffs_table entry */
    ffs_table[frame_index].pid = -1;
    ffs_table[frame_index].vpn = 0;
    ffs_table[frame_index].used = FALSE;

    /* Add frame back to free pool using existing freeFrame */
    freeFrame(frame_addr);

    /* Decrement ffs_count */
    ffs_count--;

    restore(mask);
}

/*
 * Purpose: Free all FFS frames owned by a process (on process exit)
 * Params:  pid of exiting process
 * Actions:
 *   - Scan ffs_table for entries with matching pid
 *   - Free each matching frame
 */
void free_process_ffs(pid32 pid) {
    intmask mask;
    int i;
    uint32 ffs_base_addr;
    uint32 *frame_addr;
    int freed_count = 0;

    mask = disable();

    #ifdef FFS_CLEANUP_DEBUG
    kprintf("[free_process_ffs] Starting cleanup for P%d\n", pid);
    #endif

    /* Calculate FFS base address */
    ffs_base_addr = (XINU_PAGES + MAX_PT_SIZE) * PAGE_SIZE;

    /* Scan all FFS frames */
    for (i = 0; i < MAX_FFS_SIZE; i++) {
        /* Check if this frame belongs to the exiting process */
        if (ffs_table[i].used && ffs_table[i].pid == pid) {

            #ifdef FFS_CLEANUP_DEBUG
            kprintf("[free_process_ffs] Freeing frame %d for P%d\n", i, pid);
            #endif

            /* Convert frame index to physical address */
            frame_addr = (uint32 *)(ffs_base_addr + (i * PAGE_SIZE));

            /* Clear ffs_table entry */
            ffs_table[i].pid = -1;
            ffs_table[i].vpn = 0;
            ffs_table[i].used = FALSE;

            /* Add frame back to free pool */
            freeFrame(frame_addr);

            /* Decrement ffs_count */
            ffs_count--;

            #ifdef FFS_CLEANUP_DEBUG
             freed_count++;  /* ADD THIS */
            if (freed_count % 1000 == 0) {  /* ADD THIS */
                kprintf("[free_process_ffs] freed %d frames...\n", freed_count);
            }
            #endif

        }
    }

    #ifdef FFS_CLEANUP_DEBUG
    kprintf("[free_process_ffs] Done cleaning up P%d\n", pid);
    #endif

    restore(mask);
}
