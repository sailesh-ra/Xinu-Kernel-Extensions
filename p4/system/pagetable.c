#include <xinu.h>

/* Debug control */
//#define FREEPAGE_DEBUG

void initialize_system_paging() {
    int pd_index, pt_index;
    uint32 total_pages = XINU_PAGES + MAX_PT_SIZE + MAX_FFS_SIZE;
    uint32 pages_per_pt = PAGE_SIZE / 4;  // 1024 pages per page table
    uint32 num_pdes_needed = (total_pages + pages_per_pt - 1) / pages_per_pt;  // Ceiling division

    uint32 curr_page = 0;
    pd_base_addr = (pd_t *)getpage();
    uint32* page_directory_addr = (uint32 *)pd_base_addr;
    uint32 zero_pde_entry = encode_page_directory_entry((pd_t){
        .pd_pres = 0,
        .pd_write = 1,
        .pd_user = 0,
        .pd_pwt = 0,
        .pd_pcd = 0,
        .pd_acc = 0,
        .pd_mbz = 0,
        .pd_fmb = 0,
        .pd_global = 0,
        .pd_valid = 0,
        .pd_avail = 0,
        .pd_base = 0
    });

    // Allocate and set up only the needed PDEs with page tables
    for (pd_index = 0; pd_index < num_pdes_needed; pd_index++) {
        uint32* page_table_addr = getpage();
        for (pt_index = 0; pt_index < pages_per_pt; pt_index++) {
            if (curr_page < total_pages) {
                page_table_addr[pt_index] = encode_page_table_entry((pt_t){
                    .pt_pres = 1,
                    .pt_write = 1,
                    .pt_user = 0,
                    .pt_pwt = 0,
                    .pt_pcd = 0,
                    .pt_acc = 0,
                    .pt_dirty = 0,
                    .pt_mbz = 0,
                    .pt_global = 0,
                    .pt_valid = 1,
                    .pt_avail = 0,
                    .pt_base = curr_page
                });
                curr_page++;
            } else {
                page_table_addr[pt_index] = 0;  // Not present
            }
        }
        page_directory_addr[pd_index] = encode_page_directory_entry((pd_t){
            .pd_pres = 1,
            .pd_write = 1,
            .pd_user = 0,
            .pd_pwt = 0,
            .pd_pcd = 0,
            .pd_acc = 0,
            .pd_mbz = 0,
            .pd_fmb = 0,
            .pd_global = 0,
            .pd_valid = 1,
            .pd_avail = 0,
            .pd_base = ((uint32)page_table_addr) >> VIRT_PG_OFFSET
        });
    }

    // Set the remaining PDEs to not present
    for (pd_index = num_pdes_needed; pd_index < pages_per_pt; pd_index++) {
        page_directory_addr[pd_index] = zero_pde_entry;
    }

    write_cr3((unsigned long int)pd_base_addr);
}

uint32* getpage() {
    // TODO : Implement get page function.
    struct	memblk	*prev, *curr, *leftover;
    intmask	mask;			/* Saved interrupt mask		*/
    mask = disable();
    prev = &pgmemlist;
	curr = pgmemlist.mnext;

    if (curr == NULL) {
        kprintf("PageTable Handler : PT memory full");
        restore(mask);
        return (uint32 *)SYSERR;
    }

    // Allocate one page (4096 bytes)
    if (curr->mlength == PAGE_SIZE) {
        prev->mnext = curr->mnext;
        pgmemlist.mlength -= PAGE_SIZE;
        restore(mask);
        return (uint32 *)(curr);
    } else if (curr->mlength > PAGE_SIZE) {
        leftover = (struct memblk *)((uint32) curr + PAGE_SIZE);
        prev->mnext = leftover;
        leftover->mnext = curr->mnext;
        leftover->mlength = curr->mlength - PAGE_SIZE;
        pgmemlist.mlength -= PAGE_SIZE;
        restore(mask);
        return (uint32 *)(curr);
    } else {
        kprintf("PageTable Handler : Something Seriously wrong. This should not happen");
        restore(mask);
        return (uint32 *)SYSERR;
    }

}

/*------------------------------------------------------------------------
 * freepage - Free a page table/directory back to pgmemlist (with coalescing)
 *------------------------------------------------------------------------
 */
syscall freepage(uint32 *page_addr) {
    intmask mask;
    struct memblk *prev, *curr, *block;
    uint32 top;
    
    mask = disable();
    
    #ifdef FREEPAGE_DEBUG
    kprintf("[freepage] Freeing page at 0x%08x\n", page_addr);
    #endif
    
    /* Treat the page as a memblk */
    block = (struct memblk *)page_addr;
    
    /* Walk list to find insertion point (address-ordered) */
    prev = &pgmemlist;
    curr = pgmemlist.mnext;
    
    while (curr != NULL && curr < block) {
        prev = curr;
        curr = curr->mnext;
    }
    
    /* Calculate top of previous block */
    if (prev == &pgmemlist) {
        top = 0;
    } else {
        top = (uint32)prev + prev->mlength;
    }
    
    #ifdef FREEPAGE_DEBUG
    kprintf("[freepage] prev=0x%08x, curr=0x%08x, top=0x%08x\n", prev, curr, top);
    #endif
    
    /* Coalesce with previous block if adjacent */
    if (prev != &pgmemlist && top == (uint32)block) {
        #ifdef FREEPAGE_DEBUG
        kprintf("[freepage] Coalescing with previous block\n");
        #endif
        prev->mlength += PAGE_SIZE;
        block = prev;
    } else {
        /* Insert new node */
        #ifdef FREEPAGE_DEBUG
        kprintf("[freepage] Inserting new node\n");
        #endif
        block->mnext = curr;
        block->mlength = PAGE_SIZE;
        prev->mnext = block;
    }
    
    /* Coalesce with next block if adjacent */
    if (curr != NULL && ((uint32)block + block->mlength) == (uint32)curr) {
        #ifdef FREEPAGE_DEBUG
        kprintf("[freepage] Coalescing with next block\n");
        #endif
        block->mlength += curr->mlength;
        block->mnext = curr->mnext;
    }
    
    /* Update total free memory */
    pgmemlist.mlength += PAGE_SIZE;
    
    #ifdef FREEPAGE_DEBUG
    kprintf("[freepage] Done. Total free pgmem: %d bytes\n", pgmemlist.mlength);
    #endif
    
    restore(mask);
    return OK;
}