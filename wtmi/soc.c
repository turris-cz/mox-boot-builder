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
#include "reload.h"
#include "ddr.h"
#include "board.h"
#include "soc.h"
#include "div64.h"

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

static const struct reg_val reset_a53_regs[] = {
	/* CPU Configuration */
	{ 0xc000d00c, 0x66ffc030 },
	/* Reset ATF Mailbox */
	{ 0x64000400, 0 },
};

static void reset_peripherals(void)
{
	/* Force INTn pin on PHY down to workaround reset bug */
	mdio_begin();
	mdio_write(1, 22, 3);
	mdio_write(1, 18, 0xc985);
	mdio_write(1, 22, 0);
	mdio_write(1, 0, BIT(15));
	udelay(1000000);
	mdio_end();

	/* North Bridge peripherals reset */
	writel(BIT(10) | BIT(6) | BIT(3), NB_RESET);
	udelay(1000);
	writel(0x7fcff, NB_RESET);
	udelay(1000);

	write_reg_vals(reset_nb_regs);

	/* South Bridge peripheral reset */
	writel(0, SB_RESET);
	udelay(1000);
	writel(0xf3c, SB_RESET);
	udelay(1000);

	write_reg_vals(reset_sb_regs);

	/* PCIE/GBE0 PHY */
	writel(0x122, 0xc001f382);
	udelay(10);
	writel(0x21, 0xc001f382);
	udelay(10);

	/* USB3/GBE1 PHY */
	writel(0x122, 0xc005c382);
	udelay(10);
	writel(0x21, 0xc005c382);
	udelay(10);

	/* USB3/SATA PHY */
	write_indirect(0x122, 0x3c1);
	udelay(10);
	write_indirect(0x21, 0x3c1);
	udelay(10);
}

static void core0_reset_cycle(void)
{
	writel(0xa1b2c3d4, 0xc000d060);
}

static void core1_reset(int reset)
{
	setbitsl(0xc000d00c, reset ? 0 : BIT(31), BIT(31));
}

#ifdef A53_HELPER

#include "a53_helper/a53_helper.c"
#define A53_HELPER_ADDR		0x10000000
#define A53_HELPER_DONE		0x10010000
#define A53_HELPER_ARGS		0x10010004

static void _run_a53_helper(u32 cmd, int argc, ...)
{
	va_list ap;
	int i;

	memcpy((void *)AP_RAM(A53_HELPER_ADDR), a53_helper_code, sizeof(a53_helper_code));

	writel(cmd, AP_RAM(A53_HELPER_ARGS));
	va_start(ap, argc);
	for (i = 0; i < argc; ++i)
		writel(va_arg(ap, u32), AP_RAM(A53_HELPER_ARGS + 4*i + 4));
	va_end(ap);

	writel(0, AP_RAM(A53_HELPER_DONE));

	start_ap_at(A53_HELPER_ADDR);

	while (!readl(AP_RAM(A53_HELPER_DONE)))
		udelay(1000);

	core1_reset(1);
	core0_reset_cycle();
}

static inline __attribute__((__gnu_inline__)) __attribute__((__always_inline__)) void run_a53_helper(u32 cmd, ...)
{
	_run_a53_helper(cmd, __builtin_va_arg_pack_len(), __builtin_va_arg_pack());
}

#endif /* A53_HELPER */

void start_ap_workaround(void)
{
	u32 i;

	start_ap();

	for (i = 0; i < 4000; ++i) {
		udelay(100);
		if (readl(PLAT_MARVELL_MAILBOX_BASE))
			break;
	}

	if (i == 1000) {
		core1_reset(1);
		core0_reset_cycle();
	}
}

extern u8 next_wtmi[];

static void reload_secure_firmware(void)
{
	u32 len;

	if (load_image(WTMI_ID, next_wtmi, &len)) {
		disable_irq();
		disable_systick();
		while (1)
			wait_for_irq();
	}

	do_reload(next_wtmi, len);

	/* Should not reach here */
}

static void reset_mox(void)
{
	disable_irq();
	disable_systick();

	core1_reset(1);
	core0_reset_cycle();
	udelay(1000);

	reset_peripherals();
	udelay(1000);

	/* write magic value into WARM RESET register */
	writel(0x1d1e, 0xc0013840);

	/*
	 * If we get here, warm reset failed. Try to reload secure firmware from
	 * SPI. Note: this may fail when loading U-Boot, because we did not
	 * manage to reset GIC correctly.
	 */
	udelay(10000);

	write_reg_vals(reset_a53_regs);

	/* We do not need to check return value here. New secure firmware should
	 * run without OBMI image. */
	load_image(OBMI_ID, (void *)AP_RAM(ATF_ENTRY_ADDRESS), NULL);

	reload_secure_firmware();
}

void reset_soc(void)
{
	if (get_board() == Turris_MOX)
		reset_mox();

	/* write magic value into WARM RESET register */
	writel(0x1d1e, 0xc0013840);

	/* Should not reach here */
	while (1)
		wait_for_irq();
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
#ifdef A53_HELPER
		memcpy((void *)AP_RAM(addr), a53_helper_code,
		       sizeof(a53_helper_code));
#else
		printf("a53_helper not supported in this build\n");
		return;
#endif
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

void mox_wdt_workaround(void)
{
	u32 reg, lo, hi;

	reg = readl(0xc000d064);
	if (!(reg & BIT(1)))
		return;

	reg = readl(0xc0008310);
	if (!(reg & BIT(1)))
		return;

	lo = readl(0xc0008314);
	hi = readl(0xc0008318);
	if (hi || (lo & 0xff000000))
		return;

	lo *= (reg >> 8) & 0xff;
	if (lo < 25000 * 1000)
		reset_soc();
}

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
		do_div(cntr, 25000);
		printf("Counter value: %llu ms\n", cntr);
	}
}

DEBUG_CMD("info", "Show some CPU info", info);

DECL_DEBUG_CMD(ddrtest)
{
	u32 reg, cnt, addr, i, inc_by = 4;

	if (argc > 1) {
		if (!strcmp(argv[1], "1M"))
			inc_by = 1 << 20;
		else
			goto usage;
	}

	reg = (readl(0xc0000200) >> 16) & 0x1f;
	if (reg < 7 || reg > 16) {
		printf("Unknown DDR memory size\n");
		return;
	}

	cnt = (0x4000 << reg) / (inc_by / 4);

	printf("Writing data\n");

	for (addr = 0, i = 0; i < cnt; ++i, addr += inc_by) {
		if (!(addr & 0x3fffffff)) {
			printf("Remapping %08x\n", addr);
			rwtm_win_remap(0, addr);
		}

		writel((addr & 4) ? ~addr : addr, (addr & 0x3fffffff) + 0x60000000);

		if (!(addr & 0xfffff))
			printf("%u MiB written\r", addr >> 20);
	}

	printf("\n\nNow testing\n");

	for (addr = 0, i = 0; i < cnt; ++i, addr += inc_by) {
		if (!(addr & 0x3fffffff)) {
			printf("Remapping %08x\n", addr);
			rwtm_win_remap(0, addr);
		}

		if (readl((addr & 0x3fffffff) + 0x60000000) != ((addr & 4) ? ~addr : addr))
			printf("Error at address %08x\n", addr);

		if (!(addr & 0xfffff))
			printf("%u MiB tested\r", addr >> 20);
	}

	rwtm_win_remap(0, 0);

	printf("\nDone.\n");
	return;
usage:
	printf("usage: ddrtest [1M]\n");
}

DEBUG_CMD("ddrtest", "ddr test", ddrtest);
