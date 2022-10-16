#include "disk/mbr.h"

#define MBR_BASE 0x7c00
#define MBR_OFFSET 446

struct PTE *mbr_read_pte() { return (struct PTE *)(MBR_BASE + MBR_OFFSET); }