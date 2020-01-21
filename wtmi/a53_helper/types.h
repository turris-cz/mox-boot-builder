#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned long long u_register_t;
typedef unsigned long long size_t;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

#define NULL ((void *)0)

#define MIN(a, b) ({ __auto_type _a = (a); __auto_type _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) ({ __auto_type _a = (a); __auto_type _b = (b); _a > _b ? _a : _b; })

#endif /* !__TYPES_H__ */
