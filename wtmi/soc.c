#include "types.h"
#include "io.h"
#include "irq.h"
#include "mbox.h"
#include "spi.h"
#include "string.h"
#include "mdio.h"
#include "gpio.h"
#include "printf.h"
#include "debug.h"

#if 0
#include "a53_helper/a53_helper.c"

DECL_DEBUG_CMD(kick)
{
	u32 addr = ATF_ENTRY_ADDRESS;
	enum { None, Helper, Uboot } load = None;

	if (argc > 1 && number(argv[1], &addr)) {
		if (argv[1][0] == 'h') {
			load = Helper;
		} else if (argv[1][0] == 'u') {
			load = Uboot;
		} else {
			goto usage;
		}
		++argv;
		--argc;

		if (argc > 1 && number(argv[1], &addr))
			return;
	}

	switch (load) {
	case Helper:
		memcpy((void *)(AP_RAM + addr), a53_helper_code,
		       sizeof(a53_helper_code));
		break;
	case Uboot:
		spi_nor_read(&nordev, (void *)(AP_RAM + addr), 0x20000,
			     0x160000);
		break;
	default:
		break;
	}

	start_ap_at(addr);
	return;
usage:
	puts("usage: kick [uboot|helper] [addr]\n");
	puts("       starts main CPU at address addr (default 0x04100000)\n");
	puts("       if argument 1 is uboot, loads U-Boot from SPI-NOR\n");
	puts("       if argument 1 is helper, loads a53_helper (compiled-in binary)\n");
}

DEBUG_CMD("kick", "Kick AP", kick);

DECL_DEBUG_CMD(swrst)
{
	u32 reg;

	mdio_write(1, 22, 0);
	mdio_write(1, 0, BIT(15));

	writel(0xa1b2c3d4, 0xc000d060);
	writel(0x4000, 0xc000d010);
	writel(0x66ffc030, 0xc000d00c);
//	setbitsl(0xc000d00c, 0, BIT(31));

	writel(0xffffffff, 0xc0008a04);
	writel(0xffffffff, 0xc0008a08);
	writel(0, 0xc0008a0c);
	writel(0, 0xc0008a00);

	writel(0xffffffff, 0xc0008a14);
	writel(0xffffffff, 0xc0008a18);
	writel(0xf003c000, 0xc0008a1c);
	writel(0, 0xc0008a10);

	for (reg = 0xc0008a30; reg < 0xc0008a64; reg += 4)
		writel(0, reg);

	writel(0, 0xc000d064);
	writel(0x0200, 0xc0008300);
	writel(0x0200, 0xc0008310);
	writel(0x0200, 0xc0008320);
	writel(0x0204, 0xc0008330);
	writel(0x0205, 0xc0008330);

	/* clocks to default values */
	writel(0x03cfccf2, 0xc0013000);
	writel(0x1296202c, 0xc0013004);
	writel(0x21061aa9, 0xc0013008);
	writel(0x20543084, 0xc001300c);
	writel(0x00009fff, 0xc0013010);
	writel(0x00000000, 0xc0013014);
	writel(0x003f8f40, 0xc0018000);
	writel(0x02515508, 0xc0018004);
	writel(0x00300880, 0xc0018008);
	writel(0x00000540, 0xc001800c);
	writel(0x000007aa, 0xc0018010);
	writel(0x00180000, 0xc0018014);
}

DEBUG_CMD("swrst", "CPU Software Reset", swrst);
#endif

DECL_DEBUG_CMD(info)
{
	u32 reg;

	printf("Uptime: %u seconds\n", jiffies / HZ);

	printf("Led: %s\n", gpio_get_val(57) ? "off" : "on");

	reg = readl(0xc000d064);
	printf("SOC Watchdog: %s\n", reg ? "active" : "inactive");

	reg = readl(0xc0008310);
	printf("SOC Watchdog counter: %s, %s\n",
	       (reg & BIT(0)) ? "enabled" : "disabled",
	       (reg & BIT(1)) ? "active" : "inactive");
	if (reg & BIT(1)) {
		u64 cntr;
		cntr = readl(0xc0008314);
		cntr |= ((u64)readl(0xc0008318)) << 32;
		cntr *= (reg >> 8) & 0xff;
		cntr /= 25000;
		printf("Counter value: %d ms\n", (u32)cntr);
	}

#define l(a) printf(#a " = %08x\n", readl((a)))

	l(0xc000d00c);
	l(0xc000d010);
	l(0xc000d044);
	l(0xc000d048);
	l(0xc000d0a0);
	l(0xc000d0a4);
	l(0xc000d0b0);
	l(0xc000d0c0);
}

DEBUG_CMD("info", "Show some CPU info", info);
