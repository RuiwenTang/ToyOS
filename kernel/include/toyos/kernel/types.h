/*
 * types.h — fixed-width types for freestanding kernel code
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_TYPES_H
#define TOYOS_KERNEL_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * container_of - Get the containing struct from a pointer to one of its
 * members. Same as ToyOS64 (list.h's intrusive lists are built on it).
 */
#define container_of(ptr, type, member) \
  ((type*)((char*)(ptr) - __builtin_offsetof(type, member)))

#endif /* TOYOS_KERNEL_TYPES_H */
