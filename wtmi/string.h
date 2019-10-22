#ifndef _STRING_H_
#define _STRING_H_

#include "types.h"

void *memset(void *dest, int c, u32 n);
void *memcpy(void *dest, const void *src, u32 n);
void *memmove(void *dest, const void *src, u32 n);
int strcmp(const char *p1, const char *p2);
int strncmp(const char *p1, const char *p2, u32 n);
u32 strlen(const char *s);
u32 strnlen(const char *s, u32 m);

#endif /* _STRING_H_ */
