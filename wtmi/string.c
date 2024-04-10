#include "types.h"

void bzero(void *dest, size_t n)
{
	u8 *d = dest;

	while (n--)
		*d++ = 0;
}

void *memset(void *dest, int c, size_t n)
{
	u8 *d = dest;

	while (n--)
		*d++ = c;

	return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	u8 *d = dest, *e = d + n;
	const u8 *s = src;

	while (d < e)
		*d++ = *s++;

	return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
	if (dest == src) {
		return dest;
	} else if (dest < src || src + n <= dest) {
		return memcpy(dest, src, n);
	} else {
		u8 *d = dest + n, *e = dest;
		const u8 *s = src + n;

		while (d > e)
			*d-- = *s--;

		return dest;
	}
}

int memcmp(const void *_p1, const void *_p2, size_t n)
{
	const s8 *p1 = _p1, *p2 = _p2;
	s8 d;

	while (n--) {
		d = *p1++ - *p2++;
		if (d)
			return d;
	}

	return 0;
}

void *memchr(void *s, int c, size_t n)
{
	while (n--) {
		if (*(u8 *)s == c)
			return s;
		s++;
	}

	return NULL;
}

int strcmp(const char *p1, const char *p2)
{
	while (*p1 == *p2) {
		if (*p1 == '\0')
			return 0;
		++p1, ++p2;
	}

	return *p1 - *p2;
}

int strncmp(const char *p1, const char *p2, size_t n)
{
	while (n && *p1 == *p2) {
		if (*p1 == '\0')
			return 0;
		++p1, ++p2, --n;
	}

	return n ? (*p1 - *p2) : 0;
}

size_t strlen(const char *s)
{
	size_t res = 0;

	while (*s++)
		++res;

	return res;
}

size_t strnlen(const char *s, size_t n)
{
	u32 res = 0;

	while (*s++ && n)
		++res, --n;

	return res;
}
