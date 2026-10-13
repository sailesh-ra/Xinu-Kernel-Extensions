/* paging.h */

#ifndef _PAGING_H_
#define _PAGING_H_

/* Macros */
#define XINU_PAGES      8192    /* number of pages used by default by Xinu               */
#define PAGE_SIZE       4096    /* number of bytes per page                              */
#define MAX_FFS_SIZE    (16*1024) /* size of FFS space  (in frames)                        */
#define MAX_SWAP_SIZE   (32*1024) /* size of swap space  (in frames)                       */
#define MAX_PT_SIZE     1024    /* size of space used for page tables (in frames)        */

#define VIRT_PG_OFFSET  12      /* number of bits for page offset in virtual address     */
#define VIRT_PT_OFFSET  10      /* number of bits for page table offset in virtual address  */
#define VIRT_PD_OFFSET  10      /* number of bits for page directory offset */

/* Swapping control */
#define SWAPPING_ON 1        /* Enable swapping support */
#define DEBUG_SWAPPING      /* Enable debug output */

/* Structure for a page directory entry */

typedef struct {

  unsigned int pd_pres	: 1;		/* page table present?		*/
  unsigned int pd_write : 1;		/* page is writable?		*/
  unsigned int pd_user	: 1;		/* is use level protection?	*/
  unsigned int pd_pwt	: 1;		/* write through cachine for pt?*/
  unsigned int pd_pcd	: 1;		/* cache disable for this pt?	*/
  unsigned int pd_acc	: 1;		/* page table was accessed?	*/
  unsigned int pd_mbz	: 1;		/* must be zero			*/
  unsigned int pd_fmb	: 1;		/* four MB pages?		*/
  unsigned int pd_global: 1;		/* global (ignored)		*/
  unsigned int pd_valid : 1;		/* Is page dir entry valid? We may not need this. Keeping it for now		*/
  unsigned int pd_avail : 2;		/* for programmer's use		*/
  unsigned int pd_base	: 20;		/* location of page table?	*/
} pd_t;

/* Structure for a page table entry */

typedef struct {

  unsigned int pt_pres	: 1;		/* page is present?		*/
  unsigned int pt_write : 1;		/* page is writable?		*/
  unsigned int pt_user	: 1;		/* is use level protection?	*/
  unsigned int pt_pwt	: 1;		/* write through for this page? */
  unsigned int pt_pcd	: 1;		/* cache disable for this page? */
  unsigned int pt_acc	: 1;		/* page was accessed?		*/
  unsigned int pt_dirty : 1;		/* page was written?		*/
  unsigned int pt_mbz	: 1;		/* must be zero			*/
  unsigned int pt_global: 1;		/* should be zero in 586	*/
  unsigned int pt_valid : 1;		/* Valid bit - Programmed	*/
  unsigned int pt_swap   : 1;   /* Bit 10: Swapped (OS use)*/
  unsigned int pt_avail : 1;		/* for programmer's use*/
  unsigned int pt_base	: 20;		/* location of page?		*/
} pt_t;

/* Structure for a virtual address */

typedef struct{
  unsigned int pg_offset : 12;		/* page offset			*/
  unsigned int pt_offset : 10;		/* page table offset		*/
  unsigned int pd_offset : 10;		/* page directory offset	*/
} virt_addr_t;

/* Structure for a physical address */

typedef struct{
  unsigned int fm_offset : 12;		/* frame offset			*/
  unsigned int fm_num : 20;		/* frame number			*/
} phy_addr_t;

/* Virtual memory block for heap allocation tracking */
struct vmemblk{
    struct vmemblk *vnext;    /* Pointer to next free virtual block */
    uint32 vaddr;             /* Starting virtual address */
    uint32 vlength;           /* Size of block in bytes */
};

/* Reverse mapping: PFN → (pid, vpn) */
/* FFS tracking entry */
struct ffs_entry {
    pid32   pid;
    uint32  vpn;
    bool8   used;
};

/* Swap tracking entry */
struct swap_entry {
    pid32   pid;
    uint32  vpn;
};

/* Functions to manipulate control registers and enable paging (see control_reg.c)	 */

unsigned long read_cr0(void);

unsigned long read_cr2(void);

unsigned long read_cr3(void);

unsigned long read_cr4(void);

void write_cr0(unsigned long n);

void write_cr3(unsigned long n);

void write_cr4(unsigned long n);

void enable_paging();


extern uint32*  getpage(void);              /* Allocate from pgmemlist */

/* FFS memory allocation functions */  
extern uint32*  getFrame(void);             /* Allocate from ffs_memlist */
extern void     freeFrame(uint32* frame);   /* Free to ffs_memlist */

/* Initialization functions */
extern void     initialize_system_paging(void);  /* Init system paging */
extern void     initialize_ffs(void);            /* Init FFS pool */

/* Debug functions (required by test cases) */
extern uint32   free_ffs_pages(void);                 
extern uint32   allocated_virtual_pages(pid32 pid);   
extern uint32   used_ffs_frames(pid32 pid);

#endif