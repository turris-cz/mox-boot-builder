/* Glue between u-boot and upstream zlib */
#ifndef __GLUE_ZLIB_H__
#define __GLUE_ZLIB_H__

typedef unsigned long uintptr_t;

#include "types.h"

static inline unsigned short get_unaligned(const unsigned short *p)
{
	const struct { unsigned short x; } __attribute__((packed)) *s = (const void *)p;
	return s->x;
}

extern u32 crc32(u32 crc, const u8 *p, u32 len);

#include "malloc.h"
#include "zlib_defs.h"

/* avoid conflicts */
#undef OFF
#undef ASMINF
#undef POSTINC
#undef NO_GZIP
#define GUNZIP
#undef STDC
#undef NO_ERRNO_H

#endif
