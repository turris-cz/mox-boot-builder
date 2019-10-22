#ifndef _STRING_H_
#define _STRING_H_

static inline void *memset(void *dest, int c, u32 n)
{
	u8 *d = dest;

	while (n--)
		*d++ = c;

	return dest;
}

static inline void *memcpy(void *dest, const void *src, u32 n)
{
	u8 *d = dest, *e = d + n;
	const u8 *s = src;

	while (d < e)
		*d++ = *s++;

	return dest;
}

static inline void *memmove(void *dest, const void *src, u32 n)
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

static inline int strcmp(const char *p1, const char *p2)
{
	while (*p1 == *p2) {
		if (*p1 == '\0')
			return 0;
		++p1, ++p2;
	}

	return *p1 - *p2;
}

static inline u32 strlen(const char *s)
{
	u32 res = 0;

	while (*s++)
		++res;

	return res;
}

static inline u32 strnlen(const char *s, u32 m)
{
	u32 res = 0;

	while (*s++ && m)
		++res, --m;

	return res;
}

#endif /* _STRING_H_ */
