
#include <xinu.h>

pt_t* get_or_create_pt(pd_t *pd, uint32 vaddr)
{
	uint32	pd_index;
	pd_t	*pde;
	pt_t	*pt;
	
	pd_index = (vaddr >> 22) & 0x3FF;  // Top 10 bits
	pde = &pd[pd_index];
	
	if (pde->pd_pres == 0) {
		// Allocate new page table
		pt = (pt_t*)getpage();
		if (pt == (pt_t*)SYSERR) {
			return (pt_t*)SYSERR;
		}
		
		// Zero out the page table
		memset(pt, 0, PAGE_SIZE);
		
		// Set up PDE
		pde->pd_pres = 1;
		pde->pd_write = 1;
		pde->pd_user = 0;
		pde->pd_valid = 1;
		pde->pd_base = ((uint32)pt) >> 12;
	}
	
	// Return physical address of page table
	return (pt_t*)(pde->pd_base << 12);
}


syscall map_identity_page(pd_t *pd, uint32 vaddr)
{
	pt_t	*pt;
	uint32	pt_index;
	pt_t	*pte;
	uint32	page_vaddr;
	
	// Get or create page table
	pt = get_or_create_pt(pd, vaddr);
	if (pt == (pt_t*)SYSERR) {
		return SYSERR;
	}
	
	// Get PTE index (bits 12-21)
	pt_index = (vaddr >> 12) & 0x3FF;
	pte = &pt[pt_index];
	
	// Truncate to page boundary
	page_vaddr = vaddr & 0xFFFFF000;
	
	// Set up identity mapping
	pte->pt_pres = 1;
	pte->pt_write = 1;
	pte->pt_user = 0;
	pte->pt_valid = 1;
	pte->pt_base = page_vaddr >> 12;

	
	return OK;
}


syscall map_identity_range(pd_t *pd, uint32 start, uint32 end)
{
	uint32	vaddr;
	
	// Align to page boundaries
	start = start & 0xFFFFF000;  // Truncate to page boundary
	end = (end + 0xFFF) & 0xFFFFF000;  // Round up to page boundary
	
	for (vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
		// kprintf("Reaching here a");
		if (map_identity_page(pd, vaddr) == SYSERR) {
			return SYSERR;
		}
	}
	
	return OK;
}