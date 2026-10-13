/* vmalloc.c - vmalloc */

/*vmalloc flow:

vmalloc(nbytes)
  → Round to page size
  → First-fit search in prvmlist
  → Split/remove block from free list
  → For each page: setup PTE (valid=1, present=0)
  → Return virtual address */

#include <xinu.h>

/* Enable debug output */
//#define VMALLOC_DEBUG
//#define VMALLOC_ROLLBACK

/*------------------------------------------------------------------------
 * vmalloc - Allocate virtual heap memory (stub)
 *------------------------------------------------------------------------
 */
char *vmalloc(uint32 nbytes) {
    intmask mask;
    struct procent *prptr;
    struct vmemblk *prev, *curr;
    uint32 npages, nbytes_aligned;
    uint32 vaddr;
    pd_t *pd;
    pt_t *pt;
    uint32 i, pd_index, pt_index;
    uint32 saved_cr3;


    mask = disable();

    #ifdef VMALLOC_DEBUG
    kprintf("vmalloc: P%d requesting %d bytes\n", currpid, nbytes);
    #endif

    /* Only user processes can use vmalloc */
    prptr = &proctab[currpid];
    if (prptr->pruser != TRUE) {
        #ifdef VMALLOC_DEBUG
        kprintf("vmalloc: ERROR - not a user process\n");
        #endif
        restore(mask);
        return (char *)SYSERR;
    }

    /* Round up to page boundary */
    nbytes_aligned = (nbytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    npages = nbytes_aligned / PAGE_SIZE;

    #ifdef VMALLOC_DEBUG
    kprintf("vmalloc: aligned to %d bytes (%d pages)\n", nbytes_aligned, npages);
    kprintf("vmalloc: heap range 0x%08x - 0x%08x\n", prptr->prheapbase, prptr->prheaplimit);
    #endif

    if (npages == 0) {
        #ifdef VMALLOC_DEBUG
        kprintf("vmalloc: ERROR - 0 pages requested\n");
        #endif
        restore(mask);
        return (char *)SYSERR;
    }

    /* Check if request exceeds available virtual address space */
    if (npages > ((1024 * 1024) - XINU_PAGES)) {
        kprintf("vmalloc(): not enough contiguous heap space available!\n");
        restore(mask);
        return (char *)SYSERR;
    }

    /* Check if allocation would exceed page table space */
    uint32 num_pts_needed = (npages + 1023) / 1024;
    if (num_pts_needed > (pgmemlist.mlength / PAGE_SIZE)) {
        kprintf("vmalloc(): not enough page table space available!\n"); 
        restore(mask);
        return (char *)SYSERR;
    }

    /* Block 2: First-fit search */
    
    prev = NULL;
    curr = prptr->prvmlist;

    while (curr != NULL) {
        if (curr->vlength >= nbytes_aligned) {
            break;  /* Found suitable block */
        }
        prev = curr;
        curr = curr->vnext;
    }

    if (curr == NULL) {
        kprintf("vmalloc(): not enough contiguous heap space available!\n");
        #ifdef VMALLOC_DEBUG
        kprintf("vmalloc: ERROR - no suitable block found\n");
        #endif
        restore(mask);
        return (char *)SYSERR;
    }

    /* Block 3: Allocate from block */
    vaddr = curr->vaddr;

    if (curr->vlength == nbytes_aligned) {
        /* Exact fit - remove block from list */
        if (prev == NULL) {
            prptr->prvmlist = curr->vnext;
        } else {
            prev->vnext = curr->vnext;
        }
        freemem((char *)curr, sizeof(struct vmemblk));
    } else {
        /* Split block - adjust current block */
        curr->vaddr += nbytes_aligned;
        curr->vlength -= nbytes_aligned;
    }

    #ifdef VMALLOC_DEBUG
    kprintf("vmalloc: allocated vaddr=0x%08x (%d pages)\n", vaddr, npages);
    #endif

    /* Block 4: Setup page table entries (lazy allocation) */
    /* Must use kernel PD to access page tables in PT area */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);

    pd = prptr->prpd;

    for (i = 0; i < npages; i++) {
        uint32 page_vaddr = vaddr + (i * PAGE_SIZE);
        pd_index = (page_vaddr >> 22) & 0x3FF;
        pt_index = (page_vaddr >> 12) & 0x3FF;

        /* Create page table if needed */
        if (pd[pd_index].pd_pres == 0) {
            pt = (pt_t *)getpage();
            if (pt == (pt_t *)SYSERR) {
                kprintf("vmalloc(): not enough page table space available!\n");

                #ifdef VMALLOC_ROLLBACK
                /* Rollback: clear PTEs we already set */
                uint32 j;
                for (j = 0; j < i; j++) {
                    uint32 rollback_vaddr = vaddr + (j * PAGE_SIZE);
                    uint32 rb_pd_idx = (rollback_vaddr >> 22) & 0x3FF;
                    uint32 rb_pt_idx = (rollback_vaddr >> 12) & 0x3FF;
                    pt_t *rb_pt = (pt_t *)(pd[rb_pd_idx].pd_base << 12);
                    rb_pt[rb_pt_idx].pt_valid = 0;
                    rb_pt[rb_pt_idx].pt_pres = 0;
                }
                
                /* Rollback: restore block to free list */
                struct vmemblk *newblk = (struct vmemblk *)getmem(sizeof(struct vmemblk));
                if (newblk != (struct vmemblk *)SYSERR) {
                    newblk->vaddr = vaddr;
                    newblk->vlength = nbytes_aligned;
                    newblk->vnext = prptr->prvmlist;
                    prptr->prvmlist = newblk;
                }
                #endif

                write_cr3(saved_cr3);  /* Restore before returning */
                restore(mask);
                return (char *)SYSERR;
            }
            memset(pt, 0, PAGE_SIZE);
            pd[pd_index].pd_pres = 1;
            pd[pd_index].pd_write = 1;
            pd[pd_index].pd_user = 0;
            pd[pd_index].pd_valid = 1;
            pd[pd_index].pd_base = ((uint32)pt) >> 12;
        } else {
            pt = (pt_t *)(pd[pd_index].pd_base << 12);
        }

        #ifdef VMALLOC_DEBUG
        kprintf("[vmalloc] Setting PTE: pd[%d], pt[%d], vaddr=0x%08x\n", pd_index, pt_index, page_vaddr);
        #endif

        /* Lazy allocation: valid=1, present=0 */
        pt[pt_index].pt_pres = 0;      /* Not in memory yet */
        pt[pt_index].pt_write = 1;     /* Writable */
        pt[pt_index].pt_user = 0;
        pt[pt_index].pt_valid = 1;     /* Allocated (software bit) */
        pt[pt_index].pt_base = 0;      /* No physical frame yet */
    }
    /* Restore user's page directory */
    write_cr3(saved_cr3);

    restore(mask);
    return (char *)vaddr;
}