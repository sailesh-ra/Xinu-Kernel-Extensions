#ifndef _ADDRUTIL_H_
#define _ADDRUTIL_H_

#include <paging.h>


virt_addr_t decode_virtual_address(uint32 virtual_address);
uint32 encode_address(virt_addr_t va);

pt_t decode_page_table_entry(uint32 entry);
pd_t decode_page_directory_entry(uint32 entry);

uint32 encode_page_table_entry(pt_t pt);
uint32 encode_page_directory_entry(pd_t pd);

#endif /* _ADDRUTIL_H_ */