#ifndef _STRING_H_
#define _STRING_H_

#include "types.h"

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *p1, const void *p2, size_t n);
int strcmp(const char *p1, const char *p2);
int strncmp(const char *p1, const char *p2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);

#endif /* _STRING_H_ */
