#ifndef TOY_KERNEL_SRC_ELF_LOADER_H
#define TOY_KERNEL_SRC_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

int load_and_exec(const char* path);

#endif  // TOY_KERNEL_SRC_ELF_LOADER_H