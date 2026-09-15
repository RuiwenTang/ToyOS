/*
 * string.h — Kernel string/memory function declarations
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TOYOS_KERNEL_STRING_H
#define TOYOS_KERNEL_STRING_H

#include <toyos/kernel/types.h>

void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);

#endif /* TOYOS_KERNEL_STRING_H */
