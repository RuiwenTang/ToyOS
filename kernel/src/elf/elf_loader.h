#ifndef TOY_KERNEL_SRC_ELF_LOADER_H
#define TOY_KERNEL_SRC_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int load_and_exec(const char* path);

#ifdef __cplusplus
}
#endif

#endif  // TOY_KERNEL_SRC_ELF_LOADER_H