#ifndef __TYPES_H
#define __TYPES_H

typedef signed char		int_8;
typedef unsigned char		uint_8;
typedef signed short		int_16;
typedef unsigned short		uint_16;
typedef signed int		int_32;
typedef unsigned int		uint_32;
typedef signed long long	int_64;
typedef unsigned long long	uint_64;

typedef unsigned long long	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;
typedef signed long long	s64;
typedef signed int		s32;
typedef signed short		s16;
typedef signed char		s8;
typedef char			byte;

typedef u32			size_t;

#define	NULL			((void *)0)

#define MIN(a, b) ({ __auto_type _a = (a); __auto_type _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) ({ __auto_type _a = (a); __auto_type _b = (b); _a > _b ? _a : _b; })

#define ARRAY_SIZE(x)		(sizeof((x)) / sizeof((x)[0]))

#define maybe_unused __attribute__((unused))

static inline u32 div_round_closest_u32(u32 x, u32 d)
{
	return (x + d / 2) / d;
}

#endif /* __TYPES_H */
