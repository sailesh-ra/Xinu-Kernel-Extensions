/* paging_debug.c - Debug utilities required by test cases */

#include <xinu.h>

#define DEBUG

/*------------------------------------------------------------------------
 * free_ffs_pages - Count free frames in FFS pool
 *------------------------------------------------------------------------
 */
uint32 free_ffs_pages(void)
{
    intmask mask;
    //struct memblk *curr;
    //uint32 count = 0;
    
    mask = disable();
    
    #ifndef DEBUG
    curr = ffs_memlist.mnext;
    while (curr != NULL) {
        count += curr->mlength / PAGE_SIZE;
        curr = curr->mnext;
    }
    #endif
    
    restore(mask);
    //return count;
    return ffs_memlist.mlength / PAGE_SIZE;
}

/*------------------------------------------------------------------------
 * allocated_virtual_pages - Count virtual pages allocated by process
 *------------------------------------------------------------------------
*/
uint32 allocated_virtual_pages(pid32 pid)
{
    intmask mask;
    mask = disable();
    struct procent *prptr = &proctab[pid];
    uint32 saved_cr3;

    pd_t *process_pd_base = prptr->prpd;

    /* Switch to kernel page directory to access PT area */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);

    uint32 allocated_virt_pages = 0;
    uint32 i, j;
    for (i = 0; i < 1024; i++) {
        if (process_pd_base[i].pd_valid == 1) {
            pt_t *process_pt_base = (pt_t *)(process_pd_base[i].pd_base << 12);
            for (j = 0; j < 1024; j++) {
                if (process_pt_base[j].pt_valid == 1) {
                    allocated_virt_pages++;
                }
            }
        }
    }

    /* Restore original page directory */
    write_cr3(saved_cr3);
    restore(mask);
    return allocated_virt_pages;
}

/*------------------------------------------------------------------------
 * used_ffs_frames - Count FFS frames in use by process
 *------------------------------------------------------------------------
 */
uint32 used_ffs_frames(pid32 pid)
{
    struct procent *prptr;
    pd_t *pd;
    pt_t *pt;
    uint32 count = 0;
    uint32 i, j;
    uint32 saved_cr3;
    
    if (isbadpid(pid)) {
        return 0;
    }
    
    prptr = &proctab[pid];
    pd = prptr->prpd;

    /* Switch to kernel page directory to access PT area */
    saved_cr3 = read_cr3();
    write_cr3((uint32)pd_base_addr);
    
    if (pd == NULL) {
        return 0;
    }
    
    for (i = 0; i < 1024; i++) {
        if (pd[i].pd_pres == 0) {
            continue;
        }
        
        pt = (pt_t*)(pd[i].pd_base << 12);
        
        for (j = 0; j < 1024; j++) {
            if (pt[j].pt_pres == 0) {
                continue;
            }
            
            uint32 vaddr = (i << 22) | (j << 12);
            if (vaddr >= prptr->prheapbase && vaddr < prptr->prheaplimit) {
                count++;
            }
        }
    }

    /* Restore original page directory */
    write_cr3(saved_cr3);
    
    return count;
}