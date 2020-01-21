#include "../types.h"

#define DEST_ADDR		0x1fff0000

void reload(const void *src, u32 len)
{
	static void __attribute__((noreturn)) (*jump_to)(void);
	const u8 *s = (u8 *)src;
	u8 *d = (u8 *)DEST_ADDR;

	while (len) {
		*d++ = *s++;
		--len;
	}

	jump_to = (void *) (*(u32 *)(DEST_ADDR + 4));

	jump_to();
}
