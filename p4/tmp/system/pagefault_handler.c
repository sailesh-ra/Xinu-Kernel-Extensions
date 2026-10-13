#include <xinu.h>

// #define DEBUG_PF

void pagefault_handler()
{

    /*
     * x86 Page Fault Error Code Format:
     *
     * 31-5: Reserved (set to 0)
     *    4: I/D (Instruction/Data)
     *    3: RSVD (Reserved bits violation)
     *    2: U/S (User/Supervisor mode)
     *    1: W/R (Write/Read access)
     *    0: P (Page present/protection)
     *
     * Bit 0 (P):
     *   0: The fault was caused by a nonpresent page.
     *   1: The fault was caused by a page-level protection violation.
     *
     * Bit 1 (W/R):
     *  0: The access causing the fault was a read.
     *  1: The access causing the fault was a write.
     *
     * Bit 2 (U/S):
     *  0: The access causing the fault originated when the processor
     *     was executing in supervisor mode.
     *  1: The access causing the fault originated when the processor
     *     was executing in user mode.
     *
     * Bit 3 (RSVD):
     *  0: The fault was not caused by reserved bit violation.
     *  1: The fault was caused by reserved bits set to 1 in a page directory
     *     entry or page table entry.
     *
     * Bit 4 (I/D):
     *  0: The fault was caused by data access.
     *  1: The fault was caused by instruction fetch.
     */

    /*
    Step 1: Read CR2 to get the faulting address(Virtual address)
    Step 2: Pop from stack the error code
    Step 3: Read PDBR to get the page directory base address
    Step 4: Read PTE value for the address.
    Step 5: If Access bit is not set, allocate a new frame, update the page table entry and page directory entry as needed.
    Step 6: Restore the context and return from interrupt.
    */
    unsigned long faulting_address = read_cr2();

#ifdef DEBUG_PF
    kprintf("[PF] faulting_address=0x%08x, pid=%d\n", faulting_address, currpid);
#endif

    unsigned long error_code;
    __asm__ __volatile__("popl %0" : "=r"(error_code));

    unsigned long saved_cr3 = read_cr3();

#ifdef DEBUG_PF
    kprintf("[PF] pd_base_addr=0x%08x, switching to kernel PD\n", pd_base_addr);
#endif

    write_cr3((unsigned long int)pd_base_addr);

    virt_addr_t decoded_virt_addr = decode_virtual_address(faulting_address);
    uint32 pd_index = decoded_virt_addr.pd_offset;
    pd_t *pd = (pd_t *)saved_cr3;
    pd_t page_dir_entry = pd[pd_index];

#ifdef DEBUG_PF
    kprintf("[PF] pd_index=%d, pd_valid=%d, pd_pres=%d\n",
            pd_index, page_dir_entry.pd_valid, page_dir_entry.pd_pres);
#endif

    /*
      PD entry:
        1. V = 0 - Segmentation fault
        2. V = 1; P = 0:
            Is memory full? -> Swapping stuff
            Else : Cannot happen
        3. V = 1; P = 1;
            Start page table walk
    */

    if (page_dir_entry.pd_valid == 0)
    {
        kprintf("P%d:: SEGMENTATION_FAULT\n", currpid);
        write_cr3((unsigned long int)pd_base_addr);
        kill(currpid);
        return;
    }
    else if (page_dir_entry.pd_pres == 0)
    {
        /* This should NEVER happen - vmalloc creates page tables */
        kprintf("P%d:: ERROR - PDE not present\n", currpid);
        write_cr3((unsigned long int)pd_base_addr);
        kill(currpid);
        return;
    }
    else
    {
        // Page Table Walk
        uint32 page_table_offset = decoded_virt_addr.pt_offset;
        pt_t *page_table_entry_address = (pt_t *)((page_dir_entry.pd_base << 12) + (page_table_offset * sizeof(pt_t)));
        pt_t page_table_entry = *page_table_entry_address;

        /*
          PT entry:
            1. V = 0 - Segmentation fault
            2. V = 1; P = 0:
                Is memory full? -> Swapping stuff
                Else : Allocate frame, update PTE
            3. V = 1; P = 1;
                Should not reach here as page is already present
        */

#ifdef DEBUG_PF
        kprintf("[PF] pt_valid=%d, pt_pres=%d\n", page_table_entry.pt_valid, page_table_entry.pt_pres);
#endif

        if (page_table_entry.pt_valid == 0)
        {
            kprintf("P%d:: SEGMENTATION_FAULT\n", currpid);
            write_cr3((unsigned long int)pd_base_addr);
            kill(currpid);
            return;
        }
        else if (page_table_entry.pt_pres == 0)
        {

            /* Calculate VPN for tracking */
            uint32 vpn = faulting_address >> 12;

#ifdef DEBUG_PF
            kprintf("[PF] Allocating frame for vpn=%d\n", vpn);
#endif

#ifdef SWAPPING_ON
            /* Check if page is swapped */
            if (page_table_entry.pt_swap == 1
                //  && page_table_entry.pt_dirty == 0
                )
            {
                /* ============ SWAP-IN ============ */

                /* Get swap frame from PTE */
                int swap_frame = getswapframefromList(currpid, vpn);
                /* Free the swap frame and mark as second priority (has copy in memory) */
                swap_free[swap_frame] = TRUE;
                swap_dual[swap_frame] = TRUE;
                // swap_count--;
                /* Allocate new FFS frame (may trigger eviction) */
                uint32 *new_frame = getFrame_tracked(currpid, vpn);
                if (new_frame == (uint32 *)SYSERR)
                {
                    kprintf("P%d:: Swap-in failed - no FFS frame\n", currpid);
                    write_cr3((unsigned long int)pd_base_addr);
                    kill(currpid);
                    return;
                }

                /* Calculate frame index for debug output */
#ifdef DEBUG_SWAPPING
                if (debug_swapping > 0)
                {
                    uint32 ffs_base = (XINU_PAGES + MAX_PT_SIZE) * PAGE_SIZE;
                    int new_frame_index = ((uint32)new_frame - ffs_base) / PAGE_SIZE;
                    kprintf("swapping:: swap frame 0x%x, FFS frame 0x%x\n",
                            swap_frame, new_frame_index);
                    debug_swapping--;
                }
#endif
                if(swap_clock_hand == -1)
                {
                    swap_clock_hand = swap_frame;
                }
                else
                {
                    /* Advance clock hand for next eviction */
                    swap_clock_hand = (swap_frame + 1) % MAX_SWAP_SIZE;
                }

                /* Update PTE */
                page_table_entry.pt_base = (uint32)new_frame >> 12;
                page_table_entry.pt_pres = 1;
                page_table_entry.pt_acc = 1;
                // page_table_entry.pt_swap = 0; /* Clear swap bit since backing is freed */

                *page_table_entry_address = page_table_entry;
            }
            else
            {
                /* ============ LAZY ALLOCATION (with tracking) ============ */

                uint32 *new_frame = getFrame_tracked(currpid, vpn);
                if (new_frame == (uint32 *)SYSERR)
                {
                    kprintf("P%d:: Lazy allocation failed - no FFS frame\n", currpid);
                    write_cr3((unsigned long int)pd_base_addr);
                    kill(currpid);
                    return;
                }

                page_table_entry.pt_base = (uint32)new_frame >> 12;
                page_table_entry.pt_pres = 1;
                page_table_entry.pt_acc = 1;

                *page_table_entry_address = page_table_entry;
            }
#else
            /* ============ LAZY ALLOCATION (with tracking, no swapping) ============ */
            uint32 *new_frame = getFrame_tracked(currpid, vpn);
            if (new_frame == (uint32 *)SYSERR)
            {
                kprintf("P%d:: Lazy allocation failed - no FFS frame\n", currpid);
                write_cr3((unsigned long int)pd_base_addr);
                kill(currpid);
                return;
            }

            page_table_entry.pt_base = (uint32)new_frame >> 12;
            page_table_entry.pt_pres = 1;
            page_table_entry.pt_acc = 1;
            *page_table_entry_address = page_table_entry;
#endif
        }
    }

    write_cr3((unsigned long int)saved_cr3);
}

/*
 * Enhanced page fault handling with swap support
 *
 * Flow:
 *   1. Get faulting address from CR2
 *   2. Validate address is in heap range (else segfault)
 *   3. Get PTE for faulting address
 *   4. Check PTE state:
 *
 *      Case A: pt_valid=0
 *        → Segfault (not allocated)
 *
 *      Case B: pt_valid=1, pt_swap=0, pt_pres=0
 *        → Lazy allocation
 *        → Call getFrame_tracked(currpid, vpn)
 *        → Update PTE: pt_pres=1, pt_base=new_frame
 *
 *      Case C: pt_valid=1, pt_swap=1, pt_pres=0
 *        → Swap-in needed
 *        → Get swap_frame from pt_base
 *        → Call getFrame_tracked(currpid, vpn)
 *        → Update PTE: pt_pres=1, pt_base=new_ffs_frame
 *        → (Keep pt_swap=1 for potential re-eviction)
 *        → Print debug message if DEBUG_SWAPPING defined
 *        → Optionally free swap frame OR keep for re-eviction
 */
