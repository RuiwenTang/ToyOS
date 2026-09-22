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

/*
 * ssize_t — signed size (bitmap searches return -1). Host builds get the
 * same typedef from <sys/types.h> via the test framework; an identical
 * redeclaration is legal C.
 */
typedef ptrdiff_t ssize_t;

#endif /* TOYOS_KERNEL_TYPES_H */
