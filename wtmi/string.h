#ifndef _STRING_H_
#define _STRING_H_

static inline void *memcpy(void *dest, const void *src, u32 n)
{
	u8 *d = dest, *e = d + n;
	const u8 *s = src;

	while (d < e)
		*d++ = *s++;

	return dest;
}

#endif /* _STRING_H_ */
