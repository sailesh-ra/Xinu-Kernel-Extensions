#include <xinu.h>

/* Decode a 32-bit virtual address into its constituent bit-field components. */
virt_addr_t decode_virtual_address(uint32 virtual_address) {
    virt_addr_t va;
    va.pg_offset = virtual_address & ((1 << VIRT_PG_OFFSET) - 1); // lower 12 bits
    va.pt_offset = (virtual_address >> VIRT_PG_OFFSET) & ((1 << VIRT_PT_OFFSET) - 1); // next 10 bits
    va.pd_offset = (virtual_address >> (VIRT_PG_OFFSET + VIRT_PT_OFFSET)) & ((1 << VIRT_PD_OFFSET) - 1); // next 10 bits
    return va;
}

/* Encode virt_addr_t fields into a 32-bit virtual address. */
uint32 encode_address(virt_addr_t va) {
    return (va.pd_offset << (VIRT_PT_OFFSET + VIRT_PG_OFFSET)) |
           (va.pt_offset << VIRT_PG_OFFSET) |
           va.pg_offset;
}

pt_t decode_page_table_entry(uint32 entry) {
    pt_t pt;
    pt.pt_pres   = (entry >> 0) & 0x1;
    pt.pt_write  = (entry >> 1) & 0x1;
    pt.pt_user   = (entry >> 2) & 0x1;
    pt.pt_pwt    = (entry >> 3) & 0x1;
    pt.pt_pcd    = (entry >> 4) & 0x1;
    pt.pt_acc    = (entry >> 5) & 0x1;
    pt.pt_dirty  = (entry >> 6) & 0x1;
    pt.pt_mbz    = (entry >> 7) & 0x1;
    pt.pt_global = (entry >> 8) & 0x1;
    pt.pt_valid  = (entry >> 9) & 0x1;
    pt.pt_avail  = (entry >> 10) & 0x3;
    pt.pt_base   = (entry >> 12) & 0xFFFFF;
    return pt;
}

pd_t decode_page_directory_entry(uint32 entry) {
    pd_t pd;
    pd.pd_pres   = (entry >> 0) & 0x1;
    pd.pd_write  = (entry >> 1) & 0x1;
    pd.pd_user   = (entry >> 2) & 0x1;
    pd.pd_pwt    = (entry >> 3) & 0x1;
    pd.pd_pcd    = (entry >> 4) & 0x1;
    pd.pd_acc    = (entry >> 5) & 0x1;
    pd.pd_mbz    = (entry >> 6) & 0x1;
    pd.pd_fmb    = (entry >> 7) & 0x1;
    pd.pd_valid  = (entry >> 9) & 0x1;
    pd.pd_avail  = (entry >> 10) & 0x3;
    pd.pd_base   = (entry >> 12) & 0xFFFFF;
    return pd;
}

uint32 encode_page_table_entry(pt_t pt) {
    return (pt.pt_pres   << 0) |
           (pt.pt_write  << 1) |
           (pt.pt_user   << 2) |
           (pt.pt_pwt    << 3) |
           (pt.pt_pcd    << 4) |
           (pt.pt_acc    << 5) |
           (pt.pt_dirty  << 6) |
           (pt.pt_mbz    << 7) |
           (pt.pt_global << 8) |
           (pt.pt_valid  << 9) |
           (pt.pt_avail  << 10) |
           (pt.pt_base   << 12);
}

uint32 encode_page_directory_entry(pd_t pd) {
    return (pd.pd_pres   << 0) |
           (pd.pd_write  << 1) |
           (pd.pd_user   << 2) |
           (pd.pd_pwt    << 3) |
           (pd.pd_pcd    << 4) |
           (pd.pd_acc    << 5) |
           (pd.pd_mbz    << 6) |
           (pd.pd_fmb    << 7) |
           (pd.pd_valid  << 9) |
           (pd.pd_avail  << 10) |
           (pd.pd_base   << 12);
}

