#ifndef FFS_MANAGER_H
#define FFS_MANAGER_H

#define FFS_BASE_PAGE   (XINU_PAGES + MAX_PT_SIZE)
#define FFS_BASE_ADDR   (FFS_BASE_PAGE * PAGE_SIZE)

#define ADDR_TO_FFS_INDEX(addr)  (((uint32)(addr) - FFS_BASE_ADDR) / PAGE_SIZE)
#define FFS_INDEX_TO_ADDR(idx)   ((uint32*)(FFS_BASE_ADDR + ((idx) * PAGE_SIZE)))

uint32* getFrame();
void freeFrame(uint32* frame_addr);
void initialize_ffs();

#endif /* FFS_MANAGER_H */
