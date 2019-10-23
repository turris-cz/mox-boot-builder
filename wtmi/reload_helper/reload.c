#include "../types.h"

#define DEST_ADDR		0x1fff0000

void reload(const u32 *src, u32 len)
{
	static void __attribute__((noreturn)) (*jump_to)(void);
	u32 *dst = (u32 *) DEST_ADDR;

	while (len) {
		*dst++ = *src++;
		--len;
	}

	dst = (u32 *)DEST_ADDR;
	++dst;
	jump_to = (void *)*dst;

	jump_to();
}
