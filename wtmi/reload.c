#include "types.h"
#include "irq.h"
#include "string.h"
#include "printf.h"
#include "debug.h"

#define RELOAD_HELPER_ADDR	0x1fffff00

#include "reload_helper/reload_helper.c"

void do_reload(u32 addr, u32 len)
{
	void __attribute__((noreturn)) (*reload_helper)(const u32 *src, u32 len);

	if ((addr % 4) || (len % 4))
		return;

	memcpy((void *)RELOAD_HELPER_ADDR, reload_helper_code, sizeof(reload_helper_code));
	disable_irq();
	disable_systick();

	/* plus 1 for thumb mode */
	reload_helper = (void *)(RELOAD_HELPER_ADDR + 1);
	reload_helper((void *)addr, len / 4);
}

#if 0
DECL_DEBUG_CMD(reload)
{
	u32 addr, len;

	if (argc < 3)
		return;

	if (number(argv[1], &addr) || number(argv[2], &len))
		return;

	if ((addr % 4) || (len % 4)) {
		puts("Address and length must be multiple of 4!\n");
		return;
	}

	puts("Reloading secure firmware\n");
	do_reload(addr, len);
}

DEBUG_CMD("reload", "reload secure-firmware", reload);
#endif
