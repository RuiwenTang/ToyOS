/*
 * string.c — Kernel string/memory operations
 *
 * Freestanding implementations required by the kernel and drivers.
 * No libc dependency.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <toyos/kernel/string.h>
#include <toyos/kernel/types.h>

void* memset(void* s, int c, size_t n) {
  unsigned char* p = (unsigned char*)s;
  while (n--) *p++ = (unsigned char)c;
  return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dest;
  const unsigned char* s = (const unsigned char*)src;
  while (n--) *d++ = *s++;
  return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
  unsigned char* d = (unsigned char*)dest;
  const unsigned char* s = (const unsigned char*)src;
  if (d < s) {
    while (n--) *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--) *--d = *--s;
  }
  return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
  const unsigned char* a = (const unsigned char*)s1;
  const unsigned char* b = (const unsigned char*)s2;
  while (n--) {
    if (*a != *b) return (int)*a - (int)*b;
    a++;
    b++;
  }
  return 0;
}

size_t strlen(const char* s) {
  size_t len = 0;
  while (*s++) len++;
  return len;
}

int strcmp(const char* s1, const char* s2) {
  while (*s1 && *s1 == *s2) {
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}
