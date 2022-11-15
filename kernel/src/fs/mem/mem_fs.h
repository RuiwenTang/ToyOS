
#ifndef TOY_KERNEL_VFS_MEM_FS_H
#define TOY_KERNEL_VFS_MEM_FS_H

#include "fs/vfs.h"

FS_NODE* mem_fs_open(const char* name, uint32_t flags, uint32_t mode);

#endif  // TOY_KERNEL_VFS_MEM_FS_H