#include "types.h"
#include "irq.h"
#include "string.h"
#include "debug.h"

#define RELOAD_HELPER_ADDR	0x1fffff00

#include "reload_helper/reload_helper.c"

void do_reload(void *addr, u32 len)
{
	void __attribute__((noreturn)) (*reload_helper)(const void *src, u32 len);

	memcpy((void *)RELOAD_HELPER_ADDR, reload_helper_code, sizeof(reload_helper_code));
	disable_irq();
	disable_systick();

	/* plus 1 for thumb mode */
	reload_helper = (void *)(RELOAD_HELPER_ADDR + 1);
	reload_helper((void *)addr, len);
}

DECL_DEBUG_CMD(reload)
{
	u32 addr, len;

	if (argc < 3)
		return;

	if (number(argv[1], &addr) || number(argv[2], &len))
		return;

	printf("Reloading secure firmware\n");
	do_reload((void *)addr, len);
}

DEBUG_CMD("reload", "reload secure-firmware", reload);
