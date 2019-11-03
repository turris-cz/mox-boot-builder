#include "types.h"
#include "io.h"
#include "clock.h"
#include "irq.h"
#include "mbox.h"
#include "spi.h"
#include "string.h"
#include "mdio.h"
#include "gpio.h"
#include "debug.h"
#include "tim.h"

#define NB_RESET		0xc0012400
#define SB_RESET		0xc0018600

#define INDIR_ADDR		0xc00e0178
#define INDIR_DATA		0xc00e017c

struct reg_val {
	u32 reg;
	u32 val;
};

static void write_indirect(u32 val, u32 addr)
{
	writel(addr, INDIR_ADDR);
	writel(val, INDIR_DATA);
}

static void _write_reg_vals(const struct reg_val *p, u32 n)
{
	while (n) {
		writel(p->val, p->reg);
		++p, --n;
	}
}

#define write_reg_vals(x) _write_reg_vals((x), sizeof((x))/sizeof((x)[0]))

static const struct reg_val reset_nb_regs[] = {
	/* UART 1 default values */
	{ 0xc0012008, 0x00042000 },
	{ 0xc0012010, 0x0002c807 },
	{ 0xc0012014, 0x1e1e1e1e },

	/* NB Clocks */
	{ 0xc0013000, 0x03cfccf2 },
	{ 0xc0013004, 0x1296202c },
	{ 0xc0013008, 0x21061aa9 },
	{ 0xc001300c, 0x20543084 },
	{ 0xc0013010, 0x00009fff },
	{ 0xc0013014, 0x00000000 },

	/* NB GPIO */
	{ 0xc0013800, 0 },
	{ 0xc0013804, 0 },
	{ 0xc0013818, 0 },
	{ 0xc001381c, 0 },
	{ 0xc0013830, 0x000d17fa },
	{ 0xc0013c00, 0 },
	{ 0xc0013c04, 0 },
	{ 0xc0013c08, 0 },
	{ 0xc0013c10, 0xffffffff },
	{ 0xc0013c14, 0xf },
	{ 0xc0013c18, 0 },
	{ 0xc0013c1c, 0 },

	/* NB PWM */
	{ 0xc0015200, 0 },
	{ 0xc0015204, 0 },
	{ 0xc0015210, 0 },
	{ 0xc0015214, 0 },
	{ 0xc0015220, 0 },
	{ 0xc0015224, 0 },
	{ 0xc0015230, 0 },
	{ 0xc0015234, 0 },
	{ 0xc0015240, 0 },

	/* NB Pad Control */
	{ 0xc0015600, 0x00007eaa },
	{ 0xc0015604, 0x00e84961 },
	{ 0xc0015608, 0x00007800 },
	{ 0xc001560c, 0x000f83ff },

	/* NB Misc */
	{ 0xc0017808, 0 },
};

static const struct reg_val reset_sb_regs[] = {
	/* SB Clocks */
	{ 0xc0018000, 0x003f8f40 },
	{ 0xc0018004, 0x02515508 },
	{ 0xc0018008, 0x00300880 },
	{ 0xc001800c, 0x00000540 },
	{ 0xc0018010, 0x000007aa },
	{ 0xc0018014, 0x00180000 },

	/* SB PHY */
	{ 0xc0018300, 0x00071800 },
	{ 0xc001830c, 0x0000000e },
	{ 0xc0018310, 0 },
	{ 0xc0018328, 0x00071800 },
	{ 0xc0018334, 0x0000000e },
	{ 0xc0018338, 0 },
	{ 0xc00183fc, 0x00000011 },

	/* SB GPIO */
	{ 0xc0018800, 0 },
	{ 0xc0018818, 0 },
	{ 0xc0018830, 0x7e3b },
	{ 0xc0018c00, 0 },
	{ 0xc0018c08, 0 },
	{ 0xc0018c10, 0xffffffff },
	{ 0xc0018c18, 0 },

	/* SB Pad Control */
	{ 0xc001a400, 0x775a },
	{ 0xc001a404, 0x3043 },
	{ 0xc001a408, 0x003c },

	/* SB Misc */
	{ 0xc001e808, 0 },
	{ 0xc001e80c, 0x01000031 },
	{ 0xc001e810, 1 },
	{ 0xc001e814, 0xe7fffff0 },
	{ 0xc001e818, 0 },
	{ 0xc001e81c, 0x3f },
};

static const struct reg_val reset_soc_irqs_regs[] = {
	{ 0xc0008a04, 0xffffffff },
	{ 0xc0008a08, 0xffffffff },
	{ 0xc0008a0c, 0 },
	{ 0xc0008a00, 0 },
	{ 0xc0008a14, 0xffffffff },
	{ 0xc0008a18, 0xffffffff },
	{ 0xc0008a1c, 0 },
	{ 0xc0008a10, 0 },
};

static const struct reg_val reset_wdt_and_counters_regs[] = {
	{ 0xc000d064, 0 },
	{ 0xc0008300, 0x200 },
	{ 0xc0008310, 0x200 },
	{ 0xc0008320, 0x200 },
	{ 0xc0008330, 0x204 },
	{ 0xc0008330, 0x205 },
};

static const struct reg_val reset_a53_regs[] = {
	/* CPU SMPEN */
	{ 0xc000d010, BIT(14) },
	/* CPU Configuration */
	{ 0xc000d00c, 0x66ffc030 },
	/* Reset ATF Mailbox */
	{ 0x64000400, 0 },
};

static void write_range(u32 start, u32 end, u32 value)
{
	u32 reg;

	for (reg = start; reg < end; reg += 4)
		writel(value, reg);
}

static void reset_soc_irqs(void)
{
	write_reg_vals(reset_soc_irqs_regs);

	write_range(0xc0008a30, 0xc0008a64, 0);
	writel(0, 0xc1d00000);
	writel(0xffff, 0xc1d00080);
	write_range(0xc1d00084, 0xc1d000fc, 0);
	writel(0xffff0000, 0xc1d00180);
	write_range(0xc1d00184, 0xc1d001fc, 0xffffffff);
	write_range(0xc1d00284, 0xc1d001fc, 0xffffffff);
	write_range(0xc1d00384, 0xc1d001fc, 0xffffffff);
}

#ifdef DEBUG_UART2
#define NB_RESET_UART2		BIT(6)
#else
#define NB_RESET_UART2		0
#endif

static void reset_peripherals(void)
{
	/* disable PHY */
	mdio_write(1, 22, 0);
	mdio_write(1, 0, BIT(15));

	/* North Bridge peripherals reset */
	writel(BIT(10) | BIT(3) | NB_RESET_UART2, NB_RESET);
	wait_ns(1000000);
	writel(0x7fcff, NB_RESET);
	wait_ns(1000000);

	write_reg_vals(reset_nb_regs);

	/* South Bridge peripheral reset */
	writel(0, SB_RESET);
	wait_ns(1000000);
	writel(0xf3c, SB_RESET);
	wait_ns(1000000);

	write_reg_vals(reset_sb_regs);

	/* PCIE/GBE0 PHY */
	writel(0x122, 0xc001f382);
	wait_ns(10000);
	writel(0x21, 0xc001f382);
	wait_ns(10000);

	/* USB3/GBE1 PHY */
	writel(0x122, 0xc005c382);
	wait_ns(10000);
	writel(0x21, 0xc005c382);
	wait_ns(10000);

	/* USB3/SATA PHY */
	write_indirect(0x122, 0x3c1);
	wait_ns(10000);
	write_indirect(0x21, 0x3c1);
	wait_ns(10000);
}

static void cpu_software_reset(void)
{
	writel(0xa1b2c3d4, 0xc000d060);
}

#include "a53_helper/a53_helper.c"

static void run_a53_helper(u32 addr)
{
	memcpy((void *)AP_RAM(addr), a53_helper_code, sizeof(a53_helper_code));
	start_ap_at(addr);
	wait_ns(100000);
}

void start_ap_workaround(void)
{
	u32 i;

	start_ap();

	for (i = 0; i < 1000; ++i) {
		wait_ns(100000);
		if (readl(0x64000400))
			break;
	}

	if (i == 1000)
		cpu_software_reset();
}

void reset_soc(void)
{
	cpu_software_reset();
	write_reg_vals(reset_a53_regs);
	reset_soc_irqs();
	write_reg_vals(reset_wdt_and_counters_regs);
	reset_peripherals();
	run_a53_helper(0x10000000);
	cpu_software_reset();

	/* check return value! */
	load_image(OBMI_ID, (void *)AP_RAM(ATF_ENTRY_ADDRESS), NULL);
	start_ap_workaround();
}

DECL_DEBUG_CMD(reset)
{
	reset_soc();
}

DEBUG_CMD("reset", "Reset SOC and peripherals", reset);

DECL_DEBUG_CMD(kick)
{
	u32 addr = ATF_ENTRY_ADDRESS;
	enum { None, Helper, Uboot } load = None;

	if (argc > 1) {
		if (argv[1][0] == 'h') {
			load = Helper;
		} else if (argv[1][0] == 'u') {
			load = Uboot;
		} else if (number(argv[1], &addr)) {
			goto usage;
		}

		if (load != None && argc > 2 && number(argv[2], &addr))
			goto usage;
	}

	switch (load) {
	case Helper:
		memcpy((void *)AP_RAM(addr), a53_helper_code,
		       sizeof(a53_helper_code));
		break;
	case Uboot:
		spi_init(&nordev);
		spi_nor_read(&nordev, (void *)AP_RAM(addr), 0x20000,
			     0x160000);
		break;
	default:
		break;
	}

	start_ap_at(addr);
	return;
usage:
	printf("usage: kick [uboot|helper] [addr]\n");
	printf("       starts main CPU at address addr (default 0x04100000)\n");
	printf("       if argument 1 is uboot, loads U-Boot from SPI-NOR\n");
	printf("       if argument 1 is helper, loads a53_helper (compiled-in binary)\n");
}

DEBUG_CMD("kick", "Kick AP", kick);

DECL_DEBUG_CMD(info)
{
	u32 reg;

	printf("Uptime: %u seconds\n", jiffies / HZ);

	printf("Led: %s\n", gpio_get_val(LED_GPIO) ? "off" : "on");

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
}

DEBUG_CMD("info", "Show some CPU info", info);

DECL_DEBUG_CMD(swrst)
{
	cpu_software_reset();
}

DEBUG_CMD("swrst", "CPU Software Reset", swrst);
