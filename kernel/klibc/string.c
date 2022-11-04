#include "string.h"

void* memchr(const void* s, int c, size_t n) {
  (void)s, (void)c, (void)n;
  return NULL;
}

int memcmp(const void* s1, const void* s2, size_t n) {
  const unsigned char* p1 = s1;
  const unsigned char* p2 = s2;

  while (n--) {
    const int r = *p1++ - *p2++;
    if (r) {
      return r;
    }
  }

  return 0;
}

void* memcpy(void* s1, const void* s2, size_t n) {
  char* dest = (char*)s1;
  const char* src = (const char*)s2;
  while (n--) {
    *dest++ = *src++;
  }
  return s1;
}

void* memmove(void* s1, const void* s2, size_t n) {
  char* dest = (char*)s1;
  const char* src = (const char*)s2;

  if (dest <= src) {
    while (n--) {
      *dest++ = *src++;
    }
  } else {
    src += n;
    dest += n;
    while (n--) {
      *--dest = *--src;
    }
  }

  return s1;
}

void* memset(void* const s, const int c, size_t n) {
  const unsigned char b = (unsigned char)c;

  unsigned char* p = (unsigned char*)s;
  while (n--) {
    *p++ = b;
  }
  return s;
}

char* strcat(char* s1, const char* s2) {
  char* rc = s1;
  if (*s1) {
    while (*++s1)
      ;
  }
  while ((*s1++ = *s2++))
    ;

  return rc;
}

char* strchr(const char* s, const int c) {
  while (*s != '\0' && *s != (char)c) {
    s++;
  }

  return (*s != '\0' || c == '\0') ? (char*)s : NULL;
}

int strcmp(const char* const s1, const char* const s2) {
  const uint8_t* p1 = (uint8_t*)s1;
  const uint8_t* p2 = (uint8_t*)s2;

  while (*p1 != '\0' && *p1 == *p2) {
    p1++;
    p2++;
  }

  return *p1 - *p2;
}

int strcoll(const char* const s1, const char* s2) { return strcmp(s1, s2); }

char* strcpy(char* s1, const char* s2) {
  char* rc = s1;
  while ((*s1++ = *s2++))
    ;
  return rc;
}

size_t strlen(const char* const s) {
  const char* p = s;
  while (*p++ != '\0') {
  }
  return p - s;
}
