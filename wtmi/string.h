#ifndef _STRING_H_
#define _STRING_H_

#include "types.h"

extern void bzero(void *s, size_t n);

static inline void *memset(void *dest, int c, size_t n)
{
	return __builtin_memset(dest, c, n);
}

static inline void *memcpy(void *dest, const void *src, size_t n)
{
	return __builtin_memcpy(dest, src, n);
}

static inline void *memmove(void *dest, const void *src, size_t n)
{
	return __builtin_memmove(dest, src, n);
}

static inline int memcmp(const void *p1, const void *p2, size_t n)
{
	return __builtin_memcmp(p1, p2, n);
}

static inline int strcmp(const char *p1, const char *p2)
{
	return __builtin_strcmp(p1, p2);
}

static inline int strncmp(const char *p1, const char *p2, size_t n)
{
	return __builtin_strncmp(p1, p2, n);
}

static inline size_t strlen(const char *s)
{
	return __builtin_strlen(s);
}

extern size_t strnlen(const char *s, size_t n);

#endif /* _STRING_H_ */
