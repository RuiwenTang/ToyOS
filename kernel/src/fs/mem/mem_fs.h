
#ifndef TOY_KERNEL_VFS_MEM_FS_H
#define TOY_KERNEL_VFS_MEM_FS_H

#include "fs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

FS_NODE* mem_fs_open(const char* name, uint32_t flags, uint32_t mode);

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_VFS_MEM_FS_H
