/* vfree.c - vfree */

/*
 * vfree flow:
 *   → Validate inputs (user process, heap range, nbytes > 0)
 *   → Round to page size
 *   → Switch to kernel CR3
 *   → For each page: free FFS if pt_pres == 1, clear PTE
 *   → Restore CR3
 *   → Return block to prvmlist with coalescing
 */

#include <xinu.h>

/* DEBUG CONTROLS  */
//#define VFREE_DEBUG_STAGE1    /* Stage 1: Validation and Setup */
//#define VFREE_DEBUG_STAGE2    /* Stage 2: CR3 Switch */
//#define VFREE_DEBUG_STAGE3    /* Stage 3: VFREE_CHECK_ALLOCATED */
//#define VFREE_DEBUG_STAGE4    /* Stage 4: Clear PTEs, Free FFS */
//#define VFREE_DEBUG_STAGE5    /* Stage 5: Restore CR3 */
//#define VFREE_DEBUG_STAGE6    /* Stage 6: Coalescing */


syscall vfree(char *ptr, uint32 nbytes) {
    intmask mask;
    struct procent *prptr;
    uint32 npages, nbytes_aligned;
    uint32 saved_cr3;
    pd_t *pd;
    pt_t *pt;
    uint32 i, vaddr;
    uint32 pd_index, pt_index;
    uint32 ffs_freed_count = 0;

    /* Variables for coalescing (Stage 6) */
    struct vmemblk *prev, *curr, *newblk;
    uint32 top;

    /* ========== STAGE 1: Validation and Setup ========== */
    
    mask = disable();

    #ifdef VFREE_DEBUG_STAGE1
    kprintf("\n[vfree] P%d: ptr=0x%08x, nbytes=%d\n", currpid, ptr, nbytes);
    #endif

    /* Validate nbytes > 0 */
    if (nbytes == 0) {
        #ifdef VFREE_DEBUG_STAGE1
        kprintf("[vfree] ERROR: nbytes is 0\n");
        #endif
        restore(mask);
        return SYSERR;
    }

    /* Get process entry */
    prptr = &proctab[currpid];

    /* Validate calling process is a user process */
    if (prptr->pruser != TRUE) {
        #ifdef VFREE_DEBUG_STAGE1
        kprintf("[vfree] ERROR: not a user process\n");
        #endif
        restore(mask);
        return SYSERR;
    }

    /* Round nbytes up to page boundary */
    nbytes_aligned = (nbytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    npages = nbytes_aligned / PAGE_SIZE;

    #ifdef VFREE_DEBUG_STAGE1
    kprintf("[vfree] aligned=%d bytes, npages=%d\n", nbytes_aligned, npages);
    kprintf("[vfree] heap range: 0x%08x - 0x%08x\n", prptr->prheapbase, prptr->prheaplimit);
    #endif

    /* Validate ptr is within heap range */
    if ((uint32)ptr < prptr->prheapbase || 
        (uint32)ptr >= prptr->prheaplimit) {
        #ifdef VFREE_DEBUG_STAGE1
        kprintf("[vfree] ERROR: ptr outside heap range\n");
        #endif
        restore(mask);
        return SYSERR;
    }

    /* Validate end of range is within heap */
    if ((uint32)ptr + nbytes_aligned > prptr->prheaplimit) {
        #ifdef VFREE_DEBUG_STAGE1
        kprintf("[vfree] ERROR: range exceeds heap limit\n");
        #endif
        restore(mask);
        return SYSERR;
    }

    #ifdef VFREE_DEBUG_STAGE1
    kprintf("[vfree] Stage 1 PASSED: validation complete\n");
    #endif

    /* ========== STAGE 2: CR3 Switch ========== */

    /* Save current CR3 and switch to kernel PD */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);

    /* Get process's page directory */
    pd = prptr->prpd;

    #ifdef VFREE_DEBUG_STAGE2
    kprintf("[vfree] Stage 2: CR3 switch\n");
    kprintf("[vfree] saved_cr3=0x%08x, kernel_pd=0x%08x\n", saved_cr3, pd_base_addr);
    kprintf("[vfree] process PD=0x%08x\n", pd);
    #endif

    if (pd == NULL) {
        #ifdef VFREE_DEBUG_STAGE2
        kprintf("[vfree] ERROR: process has no page directory\n");
        #endif
        write_cr3(saved_cr3);
        restore(mask);
        return SYSERR;
    }

    #ifdef VFREE_DEBUG_STAGE2
    kprintf("[vfree] Stage 2 PASSED: CR3 switched to kernel\n");
    #endif

    /* ========== STAGE 3: Checks pages were allocated ========== */
    /* Check ALL pages have pt_valid == 1 before freeing anything */

    #ifdef VFREE_DEBUG_STAGE3
    kprintf("[vfree] Stage 3: Validating %d pages...\n", npages);
    #endif

#ifndef VFREE_CHECK_ALLOCATED  /* Checks pages were allocated */
    for (i = 0; i < npages; i++) {
        vaddr = (uint32)ptr + (i * PAGE_SIZE);
        pd_index = (vaddr >> 22) & 0x3FF;
        pt_index = (vaddr >> 12) & 0x3FF;

        #ifdef VFREE_DEBUG_STAGE3
        kprintf("[vfree]   page %d: vaddr=0x%08x, pd[%d], pt[%d]\n", 
                i, vaddr, pd_index, pt_index);
        #endif

        /* Check if page directory entry is present */
        if (pd[pd_index].pd_pres == 0) {
            #ifdef VFREE_DEBUG_STAGE3
            kprintf("[vfree] ERROR: pd[%d].pd_pres == 0\n", pd_index);
            #endif
            write_cr3(saved_cr3);
            restore(mask);
            return SYSERR;
        }

        /* Get page table */
        pt = (pt_t *)(pd[pd_index].pd_base << 12);

        /* Check if page was allocated by vmalloc */
        if (pt[pt_index].pt_valid != 1) {
            #ifdef VFREE_DEBUG_STAGE3
            kprintf("[vfree] ERROR: pt[%d].pt_valid != 1 (was %d)\n", 
                    pt_index, pt[pt_index].pt_valid);
            #endif
            write_cr3(saved_cr3);
            restore(mask);
            return SYSERR;
        }

        #ifdef VFREE_DEBUG_STAGE3
        kprintf("[vfree]   valid=1, pres=%d, base=0x%05x\n", 
                pt[pt_index].pt_pres, pt[pt_index].pt_base);
        #endif
    }
#endif  /* Checks pages were allocated */

    #ifdef VFREE_DEBUG_STAGE3
    kprintf("[vfree] Stage 3 PASSED: all %d pages validated\n", npages);
    #endif

     /* ========== STAGE 4: Clear PTEs and Free FFS Frames ========== */

    #ifdef VFREE_DEBUG_STAGE4
    kprintf("[vfree] Stage 4: Freeing %d pages...\n", npages);
    #endif

    for (i = 0; i < npages; i++) {
        vaddr = (uint32)ptr + (i * PAGE_SIZE);
        pd_index = (vaddr >> 22) & 0x3FF;
        pt_index = (vaddr >> 12) & 0x3FF;

        /* Get page table */
        pt = (pt_t *)(pd[pd_index].pd_base << 12);

        #ifdef VFREE_DEBUG_STAGE4
        kprintf("[vfree]   page %d @ 0x%08x: ", i, vaddr);
        #endif

        /* If page was accessed (present=1), free the FFS frame */
        if (pt[pt_index].pt_pres == 1) {
            uint32 frame_addr = pt[pt_index].pt_base << 12;
            
            #ifdef VFREE_DEBUG_STAGE4
            kprintf("freeing FFS frame 0x%08x, ", frame_addr);
            #endif
            
            freeFrame_tracked((uint32 *)frame_addr, currpid);
            ffs_freed_count++;

        } else {
            #ifdef VFREE_DEBUG_STAGE4
            kprintf("no FFS (never accessed), ");
            #endif
        }

        /* Clear the PTE */
        pt[pt_index].pt_valid = 0;
        pt[pt_index].pt_pres = 0;
        pt[pt_index].pt_base = 0;
        pt[pt_index].pt_write = 0;

        #ifdef VFREE_DEBUG_STAGE4
        kprintf("PTE cleared\n");
        #endif
    }

    #ifdef VFREE_DEBUG_STAGE4
    kprintf("[vfree] Stage 4 PASSED: freed %d FFS frames, cleared %d PTEs\n", 
            ffs_freed_count, npages);
    #endif

    /* ========== STAGE 5: Restore CR3 ========== */

    write_cr3(saved_cr3);

    #ifdef VFREE_DEBUG_STAGE5
    kprintf("[vfree] Stage 5 PASSED: CR3 restored to 0x%08x\n", saved_cr3);
    #endif

    /* ========== STAGE 6: Return Block to prvmlist with Coalescing ========== */

    #ifdef VFREE_DEBUG_STAGE6
    kprintf("[vfree] Stage 6: Returning block to prvmlist\n");
    kprintf("[vfree] Block: vaddr=0x%08x, size=%d bytes\n", ptr, nbytes_aligned);
    #endif

    prev = NULL;
    curr = prptr->prvmlist;

    #ifdef VFREE_DEBUG_STAGE6
    kprintf("[vfree] Walking prvmlist to find insertion point...\n");
    #endif

    while (curr != NULL && curr->vaddr < (uint32)ptr) {
        #ifdef VFREE_DEBUG_STAGE6
        kprintf("[vfree]   curr: vaddr=0x%08x, len=%d\n", curr->vaddr, curr->vlength);
        #endif
        prev = curr;
        curr = curr->vnext;
    }

    #ifdef VFREE_DEBUG_STAGE6
    if (prev != NULL) {
        kprintf("[vfree] prev: vaddr=0x%08x, len=%d\n", prev->vaddr, prev->vlength);
    } else {
        kprintf("[vfree] prev: NULL (inserting at head)\n");
    }
    if (curr != NULL) {
        kprintf("[vfree] next: vaddr=0x%08x, len=%d\n", curr->vaddr, curr->vlength);
    } else {
        kprintf("[vfree] next: NULL (inserting at tail)\n");
    }
    #endif

    /* Calculate top of previous block */
    if (prev == NULL) {
        top = 0;  /* No previous block */
    } else {
        top = prev->vaddr + prev->vlength;
    }

    /* Try to coalesce with previous block */
    if (prev != NULL && top == (uint32)ptr) {
        #ifdef VFREE_DEBUG_STAGE6
        kprintf("[vfree] Coalescing with previous block\n");
        #endif
        
        prev->vlength += nbytes_aligned;
        newblk = prev;  /* Use prev as the merged block */
    } else {
        /* Cannot coalesce with previous - create new node */
        #ifdef VFREE_DEBUG_STAGE6
        kprintf("[vfree] Creating new vmemblk node\n");
        #endif
        
        newblk = (struct vmemblk *)getmem(sizeof(struct vmemblk));
        if (newblk == (struct vmemblk *)SYSERR) {
            #ifdef VFREE_DEBUG_STAGE6
            kprintf("[vfree] ERROR: getmem failed for vmemblk\n");
            #endif
            restore(mask);
            return SYSERR;
        }
        
        newblk->vaddr = (uint32)ptr;
        newblk->vlength = nbytes_aligned;
        newblk->vnext = curr;
        
        if (prev == NULL) {
            prptr->prvmlist = newblk;  /* Insert at head */
        } else {
            prev->vnext = newblk;
        }
    }

    /* Try to coalesce with next block */
    if (curr != NULL && (newblk->vaddr + newblk->vlength) == curr->vaddr) {
        #ifdef VFREE_DEBUG_STAGE6
        kprintf("[vfree] Coalescing with next block\n");
        #endif
        
        newblk->vlength += curr->vlength;
        newblk->vnext = curr->vnext;
        
        /* Free the old vmemblk node */
        freemem((char *)curr, sizeof(struct vmemblk));
    }

    #ifdef VFREE_DEBUG_STAGE6
    kprintf("[vfree] Stage 6 PASSED: block returned to prvmlist\n");
    kprintf("[vfree] Final block: vaddr=0x%08x, len=%d\n", newblk->vaddr, newblk->vlength);
    #endif


    #ifdef VFREE_DEBUG_STAGE1
    kprintf("[vfree] SUCCESS: freed %d pages, %d FFS frames\n\n", npages, ffs_freed_count);
    #endif


    restore(mask);
    return OK;
}